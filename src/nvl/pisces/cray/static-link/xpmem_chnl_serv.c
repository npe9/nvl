/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

/* User level test procedures */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
//
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#include <dbapi.h>
#include <dballoc.h>
#include <ctype.h>

//
#include "gni_pub.h"
#include "gni_priv.h"
#include <alps/libalpslli.h>

#include "hobbes_cmd_queue.h"
#include "xemem.h"
#include "xpmem.h"
#include "pmi.h"
#include "pmi2.h"
#include "pmi_cray_ext.h"
#include "pmi_util.h"
#include "xmem_list.h"

#define PAGE_SHIFT	12
#define PAGE_SIZE (0x1UL << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))


#define NIC_ADDR_BITS    22
#define NIC_ADDR_SHIFT   (32-NIC_ADDR_BITS)
#define NIC_ADDR_MASK    0x3FFFFF
#define CPU_NUM_BITS     7
#define CPU_NUM_SHIFT    (NIC_ADDR_SHIFT-CPU_NUM_BITS)
#define CPU_NUM_MASK     0x7F
#define THREAD_NUM_BITS  3
#define THREAD_NUM_SHIFT (CPU_NUM_SHIFT-THREAD_NUM_BITS)
#define THREAD_NUM_MASK  0x7

struct memseg_list *mt = NULL;


#ifndef HEXDUMP_COLS
#define HEXDUMP_COLS 8
#endif
int ptag, cookie, devid;
char *local_addr;
int setup_ugni(void);

void
hexdump (void *mem, unsigned int len)
{
  unsigned int i, j;

  for (i = 0;
       i <
       len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0);
       i++)
    {
      /* print offset */
      if (i % HEXDUMP_COLS == 0)
	{
	  printf ("0x%06x: ", i);
	}

      /* print hex data */
      if (i < len)
	{
	  printf ("%02x ", 0xFF & ((char *) mem)[i]);
	}
      else			/* end of block, just aligning for ASCII dump */
	{
	  printf ("   ");
	}

      /* print ASCII dump */
      if (i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
	{
	  for (j = i - (HEXDUMP_COLS - 1); j <= i; j++)
	    {
	      if (j >= len)	/* end of block, not really printing */
		{
		  putchar (' ');
		}
	      else if (isprint (((char *) mem)[j]))	/* printable char */
		{
		  putchar (0xFF & ((char *) mem)[j]);
		}
	      else		/* other char */
		{
		  putchar ('.');
		}
	    }
	  putchar ('\n');
	}
    }
}


static void
get_alps_info (alpsAppGni_t * alps_info)
{
  int alps_rc = 0;
  int req_rc = 0;
  size_t rep_size = 0;

  uint64_t apid = 0;
  alpsAppLLIGni_t *alps_info_list;
  char buf[1024];

  alps_info_list = (alpsAppLLIGni_t *) & buf[0];

  alps_app_lli_lock ();

  alps_rc = alps_app_lli_put_request (ALPS_APP_LLI_ALPS_REQ_GNI, NULL, 0);
  if (alps_rc != 0)
    fprintf (stderr, "waiting for ALPS reply\n");
  alps_rc = alps_app_lli_get_response (&req_rc, &rep_size);
  if (rep_size != 0)
    {
      alps_rc = alps_app_lli_get_response_bytes (alps_info_list, rep_size);
    }

  alps_rc = alps_app_lli_put_request (ALPS_APP_LLI_ALPS_REQ_APID, NULL, 0);
  if (alps_rc != 0)
    alps_rc = alps_app_lli_get_response (&req_rc, &rep_size);
  if (alps_rc != 0)
    fprintf (stderr, "alps_app_lli_get_response failed: alps_rc=%d\n",
	     alps_rc);
  if (rep_size != 0)
    {
      alps_rc = alps_app_lli_get_response_bytes (&apid, rep_size);
      if (alps_rc != 0)
	fprintf (stderr, "alps_app_lli_get_response_bytes failed: %d\n",
		 alps_rc);
    }

  alps_app_lli_unlock ();

  memcpy (alps_info, (alpsAppGni_t *) alps_info_list->u.buf,
	  sizeof (alpsAppGni_t));
  return;
}


static uint32_t
get_cpunum (void)
{
  int i, j;
  uint32_t cpu_num = 0;

  cpu_set_t coremask;

  (void) sched_getaffinity (0, sizeof (coremask), &coremask);

  for (i = 0; i < CPU_SETSIZE; i++)
    {
      if (CPU_ISSET (i, &coremask))
	{
	  int run = 0;
	  for (j = i + 1; j < CPU_SETSIZE; j++)
	    {
	      if (CPU_ISSET (j, &coremask))
		run++;
	      else
		break;
	    }
	  if (!run)
	    {
	      cpu_num = i;
	    }
	  else
	    {
	      cpu_num = i;
	      fprintf (stdout,
		       "This thread is bound to multiple CPUs(%d).  Using lowest numbered CPU(%d).",
		       run + 1, cpu_num);
	    }
	}
    }
  return (cpu_num);
}


#define GNI_INSTID(nic_addr, cpu_num, thr_num) (((nic_addr&NIC_ADDR_MASK)<<NIC_ADDR_SHIFT)|((cpu_num&CPU_NUM_MASK)<<CPU_NUM_SHIFT)|(thr_num&THREAD_NUM_MASK))

#define FMA_WINDOW_SIZE    (1024 * 1024 * 1024L)

/*
 * allgather gather the requested information from all of the ranks.
 */

static void
allgather (void *in, void *out, int len)
{
  static int already_called = 0;
  int i;
  static int job_size = 0;
  //int my_rank;
  //char *out_ptr;
  int rc;
  char *tmp_buf;
  typedef struct {
	  uint32_t pmix_rank;
	  int32_t jobfam;
	  int32_t vpid;
	  int32_t nbytes;
  } bytes_and_rank_t;
 
   bytes_and_rank_t *p;
  printf("%s: entering already_called %d\n", __func__, already_called);
/*
  if (!already_called)
    {
      rc = PMI_Get_size (&job_size);
      assert (rc == PMI_SUCCESS);

      rc = PMI_Get_rank (&my_rank);
      assert (rc == PMI_SUCCESS);

      ivec_ptr = (int *) malloc (sizeof (int) * job_size);
      assert (ivec_ptr != NULL);
      rc = PMI_Allgather (&my_rank, ivec_ptr, sizeof (int));
      assert (rc == PMI_SUCCESS);

      already_called = 1;
      } */
  job_size = 2;
  tmp_buf = (char *) malloc (job_size * len);
  assert (tmp_buf);
  //fprintf(stderr, "second gather for pointer %p length %d\n", in, len);
  //hexdump(in, len);
  printf("%s: allgathering\n", __func__);
  rc = PMI_Allgather (in, out, len);
  printf("%s: allgathered\n", __func__);
  assert (rc == PMI_SUCCESS);

  //out_ptr = out;
  //p = tmp_buf;
  p = out;
  for (i = 0; i < job_size; i++)
    {
	    printf("%s: pmix_rank %d vpid %d jobfam %d nbytes %d\n", __func__, p[i].pmix_rank, p[i].vpid, p[i].jobfam, p[i].nbytes);
//      memcpy (&out_ptr[len * ivec_ptr[i]], &tmp_buf[i * len], len);
    }

  free (tmp_buf);
}


static void
allgatherv (void *in, int len, void *out, int *all_len)
{
  static int already_called = 0;
  int i;
  static int job_size = 0;
  //int my_rank;
  //char *out_ptr;
  int rc;
  char *tmp_buf;// , *tmp_buf2;
/*
  typedef struct {
	  uint32_t pmix_rank;
	  int32_t jobfam;
	  int32_t vpid;
	  int32_t nbytes;
  } bytes_and_rank_t;
*/ 
  // bytes_and_rank_t *p;
  printf("%s: entering already_called %d\n", __func__, already_called);
  job_size = 2;
  tmp_buf = (char *) malloc (job_size * len * 1024);
  //tmp_buf2 = (char *) malloc (job_size * len * 1024);
  assert (tmp_buf);
  //fprintf(stderr, "second gather for pointer %p length %d\n", in, len);
  //hexdump(in, len);
  printf("%s: allgatheringv in %p out %p len %d all_len %p\n", __func__, tmp_buf, out, len, all_len);
  for(i = 0; i < 2; i++)
	  all_len[i] = len;
  rc = PMI_Allgatherv (in, len, out, all_len);
  //memcpy(tmp_buf, in, len);
  //rc = PMI_Allgatherv (tmp_buf, len, tmp_buf2, all_len);
  assert (rc == PMI_SUCCESS);
  printf("%s: ", __func__);

  free (tmp_buf);
}


int
main (int argc, char *argv[], char *envp[])
{
  int device;
  int status;
  gni_nic_setattr_args_t nic_set_attr;
  gni_mem_register_args_t *mem_register_attr;
  gni_cq_create_args_t *cq_create_attr;
  gni_post_rdma_args_t post_attr;
  gni_post_descriptor_t post_desc;
  gni_nic_fmaconfig_args_t *fmaconf_arg;
  pmi2_init_args_t *init_arg;
  pmi2_job_getid_args_t *job_getid_arg;
  pmi2_info_getjobattr_args_t *info_getjobattr_arg;
  
  alpsAppGni_t alps_info;
  uint32_t nic_addr = 0;
  uint32_t cpu_num = 0;
  uint32_t thread_num = 0;
  uint32_t gni_cpu_id = 0;
  uint32_t instance;
  int rc;
  hcq_handle_t hcq = HCQ_INVALID_HANDLE;
  int fd = 0;
  fd_set rset;
  int max_fds = 0;
  //int fd_cnt = 0;
  //int ret = -1;
  xemem_segid_t fma_win, fma_put, /* fma_nc,*/ fma_get, fma_ctrl;
  xemem_segid_t /*clean_seg,*/ reg_mem_seg;
  struct xemem_addr r_addr;
  xemem_apid_t apid;
/* for PMI */
  int my_rank;
  int job_size;
  int app_num;
  int first_spawned;
  void *outbuf;
  //void *mem_mem;
  int *mallocsz;
  void *buffer;
  //void *rdma_post_buf;
  if(setup_ugni() != 0) {
	  printf("%s: failed to set up ugni\n", __func__);
  };
  printf("%s: starting server\n", __func__);
  rc = PMI2_Init (&first_spawned, &job_size, &my_rank, &app_num );
  if(rc != PMI_SUCCESS){
	  fprintf(stderr, "%s: couldn't initialize pmi rc %d\n", __func__, rc);
	  exit(1);
  }

  assert (rc == PMI_SUCCESS);

  printf("%s: initalized client\n", __func__);
  hobbes_client_init ();

  mt = list_new ();
  hcq = hcq_create_queue ("GEMINI-NEW");

  if (hcq == HCQ_INVALID_HANDLE)
    {
      fprintf (stderr, "Could not create command queue\n");
      hcq_free_queue (hcq);
      return -1;
    }

  fprintf (stderr, "my rank : %d shadow process server segid: %lu\n",
	   my_rank, hcq_get_segid (hcq));

  fd = hcq_get_fd (hcq);


  get_alps_info (&alps_info);

  rc = GNI_CdmGetNicAddress (alps_info.device_id, &nic_addr, &gni_cpu_id);

  cpu_num = get_cpunum ();

  instance = GNI_INSTID (nic_addr, cpu_num, thread_num);

  device = open ("/dev/kgni0", O_RDWR);
  if (device < 0)
    {
      fprintf (stderr, "Failed to open device\n");
      return 0;
    }

  fprintf (stderr, "Opened device\n");
  FD_ZERO (&rset);
  FD_SET (fd, &rset);
  max_fds = fd + 1;

  printf("%s: entering server loop\n", __func__);
  while (1)
    {
      int ret = 0;

      FD_ZERO (&rset);
      FD_SET (fd, &rset);
      max_fds = fd + 1;

      ret = select (max_fds, &rset, NULL, NULL, NULL);
      printf("%s: selected\n", __func__);
      if (ret == -1)
	{
	  perror ("Select Error");
	  break;
	}


      if (FD_ISSET (fd, &rset))
	{
	  hcq_cmd_t cmd = hcq_get_next_cmd (hcq);
	  uint64_t cmd_code = hcq_get_cmd_code (hcq, cmd);
	  uint32_t data_len = 0;
	  char *data_buf = hcq_get_cmd_data (hcq, cmd, &data_len);
	  //char buf[8192];

	  printf("%s: cmd_code %ld\n", __func__, cmd_code);
	  switch (cmd_code)
	    {
	    case GNI_IOC_NIC_SETATTR:
		    printf("%s: GNI_IOC_NIC_SETATTR\n", __func__);
	      nic_set_attr.modes = 0;

	      /* Configure NIC with ptag and other attributes */
	      nic_set_attr.cookie = alps_info.cookie; 
	      nic_set_attr.ptag = alps_info.ptag;
	      nic_set_attr.rank = instance;
	      status = ioctl (device, GNI_IOC_NIC_SETATTR, &nic_set_attr);
	      if (status < 0)
		{
		  fprintf (stderr, "Failed to set NIC attributes (%d)\n",
			   status);
		  return 0;
		}


	      /* configure FMA segments in XEMEM 
	         client_segid = xemem_make_signalled(NULL, 0, XPMEM_PERMIT_MODE, (void *)0600, NULL, &client_fd);
	         GNI_NIC_FMA_SET_DEDICATED(nic);
	         nic->fma_window = (void *) nic_attrs.fma_window;
	         nic->fma_window_nwc = (void *) nic_attrs.fma_window_nwc;
	         nic->fma_window_get = (void *) nic_attrs.fma_window_get;
	         nic->fma_ctrl = (void *) nic_attrs.fma_ctrl;
	       */

	      fma_win =
		      xemem_make ((void*)nic_set_attr.fma_window, FMA_WINDOW_SIZE,
			    "fma_win_seg");
	      fma_put =
		      xemem_make ((void*)nic_set_attr.fma_window_nwc, FMA_WINDOW_SIZE,
			    "fma_win_put");
	      fma_get =
		      xemem_make ((void*)nic_set_attr.fma_window_get, FMA_WINDOW_SIZE,
			    "fma_win_get");
	      fma_ctrl =
		      xemem_make ((void*)nic_set_attr.fma_ctrl, GHAL_FMA_CTRL_SIZE,
			    "fma_win_ctrl");
	      //hexdump (nic_set_attr.fma_ctrl, 64);
	      // Put the segid instead of vaddresses
	      nic_set_attr.fma_window = (uint64_t) fma_win;
	      nic_set_attr.fma_window_nwc = (uint64_t) fma_put;
	      nic_set_attr.fma_window_get = (uint64_t) fma_get;
	      nic_set_attr.fma_ctrl = (uint64_t) fma_ctrl;
	      fprintf (stderr,
		       "dump server segids  win put get ctrl %lu %lu %lu %lu\n",
		       fma_win, fma_put, fma_get, fma_ctrl);
	      hcq_cmd_return (hcq, cmd, ret, sizeof (nic_set_attr),
			      &nic_set_attr);
	      break;
	    case GNI_IOC_MEM_REGISTER:

		    printf("%s: GNI_IOC_MEM_REGISTER\n", __func__);
	      mem_register_attr = (gni_mem_register_args_t *) data_buf;
	      //gni_mem_segment_t *segment;

	      if (mem_register_attr->segments_cnt == 1)
		{
		  apid = xemem_get (mem_register_attr->address, XEMEM_RDWR);
		  if (apid <= 0)
		    {
		      printf
			("seg attach from server for MEM REGISTER  failed \n");
		      return -1;
		    }

		  r_addr.apid = apid;
		  r_addr.offset = 0;

		  buffer =
		    xemem_attach (r_addr, mem_register_attr->length, NULL);
		  if (buffer == MAP_FAILED)
		    {
		      printf ("xpmem attach for MEM REGISTER failed\n");
		      xemem_release (apid);
		      return -1;
		    }
		  list_add_element (mt, (void*)&mem_register_attr->address, (uint64_t)buffer,
				    mem_register_attr->length);
		  mem_register_attr->address = (uint64_t)buffer;
		}
	      else
		{
			//segment = mem_register_attr->mem_segments;
		}

//
	      rc = ioctl (device, GNI_IOC_MEM_REGISTER, mem_register_attr);
	      if (rc < 0)
		{
		  fprintf (stderr,
			   "Failed calling GNI_IOC_MEM_REGISTER return code %d\n",
			   rc);
		  return rc;
		}

//
	      hcq_cmd_return (hcq, cmd, ret, sizeof (gni_mem_register_args_t),
			      mem_register_attr);
	      break;

	    case GNI_IOC_POST_RDMA:

		    printf("%s: GNI_IOC_POST_RDMA\n", __func__);
	      memcpy (&post_desc, data_buf, sizeof (gni_post_descriptor_t));
	      buffer = data_buf + sizeof (gni_post_descriptor_t);
	      memcpy (&post_attr, buffer, sizeof (gni_post_rdma_args_t));

	      //              list_print(mt);
	      //              hexdump(&post_desc, sizeof(gni_post_descriptor_t));
	      buffer =
		      (void *) list_find_vaddr_by_segid (mt, (void*)&post_desc.local_addr);
	      post_desc.local_addr = (uint64_t) buffer;
/*
	      fprintf (stderr,
		       "server after local vaddr POST RDMA 0x%lx  word1 0x%016lx  word2  0x%016lx\n",
		       post_desc.local_addr,
		       post_desc.local_mem_hndl.qword1,
		       post_desc.local_mem_hndl.qword2);
	      fprintf (stderr,
		       "server remote vaddr POST RDMA 0x%lx  word1 0x%016lx  word2  0x%016lx\n",
		       post_desc.remote_addr,
		       post_desc.remote_mem_hndl.qword1,
		       post_desc.remote_mem_hndl.qword2);
*/
	      post_attr.post_desc = (gni_post_descriptor_t *) & post_desc;
	      rc = ioctl (device, GNI_IOC_POST_RDMA, &post_attr);
	      if (rc < 0)
		{
		  fprintf (stderr,
			   "Failed calling GNI_IOC_POST_RDMA reason %d\n",
			   rc);
		  return 0;
		}
	      hcq_cmd_return (hcq, cmd, ret, sizeof (rc), &rc);

	      break;

	    case GNI_IOC_CQ_CREATE:
		    printf("%s: GNI_IOC_CQ_CREATE\n", __func__);
	      cq_create_attr = (gni_cq_create_args_t *) data_buf;
	      buffer =
		(void *) list_find_vaddr_by_segid (mt,
						   (void*)&cq_create_attr->queue);
	      cq_create_attr->queue = buffer;
	      rc = ioctl (device, GNI_IOC_CQ_CREATE, cq_create_attr);
	      if (rc < 0)
		{
		  fprintf (stderr, "Failed calling GNI_IOC_CQ_CREATE\n");
		  return 0;
		}
	      hcq_cmd_return (hcq, cmd, ret, sizeof (gni_cq_create_args_t),
			      cq_create_attr);
	      break;

	    case GNI_IOC_NIC_FMA_CONFIG:
		    printf("%s: GNI_IOC_NIC_FMA_CONFIG\n", __func__);
	      fmaconf_arg = (gni_nic_fmaconfig_args_t *) data_buf;
	      rc = ioctl (device, GNI_IOC_NIC_FMA_CONFIG, fmaconf_arg);
	      if (rc < 0)
		{
		  fprintf (stderr, "Failed calling GNI_IOC_NIC_FMA_CONFIG\n");
		  return 0;
		}
	      hcq_cmd_return (hcq, cmd, ret,
			      sizeof (gni_nic_fmaconfig_args_t), fmaconf_arg);
	      break;
	    case GNI_IOC_GETJOBRESINFO:
	    {
		    gni_job_res_desc_t res_des;

		    printf("%s: getting jobres\n", __func__);
		    rc = GNI_GetJobResInfo(devid, ptag, GNI_JOB_RES_MDD, &res_des);
		    printf("%s: got jobres res_dev used %ld limit %ld\n", __func__, res_des.used, res_des.limit);
		    hcq_cmd_return(hcq, cmd, ret, sizeof(gni_job_res_desc_t), &res_des);
	    }

	    case PMI_IOC_INIT:
		    printf("%s: got PMI_IOC_INIT\n", __func__);
		    break;
	    case PMI2_IOC_INIT:
		    init_arg = (pmi2_init_args_t*)data_buf;
		    printf("%s: got PMI2_IOC_INIT spawned %d my_rank %d job_size %d appnum %d\n", __func__, first_spawned, my_rank, job_size, app_num);
		    init_arg->spawned = first_spawned;
		    init_arg->rank = my_rank;
		    init_arg->size = job_size;
		    init_arg->appnum = app_num;
		    hcq_cmd_return (hcq, cmd, ret, sizeof(pmi2_init_args_t), init_arg);
		    break;
	    case PMI2_IOC_JOB_GETID:
		    job_getid_arg = (pmi2_job_getid_args_t*)data_buf;
		    printf("%s: got PMI2_JOB_GETID calling\n", __func__);
		    printf("%s: jobid %p jobsize %d\n", __func__, job_getid_arg->jobid, job_getid_arg->jobid_size);
		    PMI2_Job_GetId(job_getid_arg->jobid, job_getid_arg->jobid_size);
		    printf("%s: got id\n", __func__);
		    printf("%s: got PMI2_JOB_GETID jobid %s jobid_size %d\n", __func__, job_getid_arg->jobid, job_getid_arg->jobid_size);
		    hcq_cmd_return (hcq, cmd, ret, job_getid_arg->jobid_size + sizeof(int), job_getid_arg);
		    break;
	    case PMI2_IOC_INFO_GETJOBATTR:
		    info_getjobattr_arg = (pmi2_info_getjobattr_args_t*)data_buf;
		    printf("%s: got PMI2_IOC_INFO_GETJOBATTR calling name %s value %p valuelen %d\n", __func__, info_getjobattr_arg->name, info_getjobattr_arg->value, info_getjobattr_arg->valuelen);
		    PMI2_Info_GetJobAttr(info_getjobattr_arg->name, info_getjobattr_arg->value, info_getjobattr_arg->valuelen, (void*)&info_getjobattr_arg->found);
		    printf("%s: got attr\n", __func__);
		    printf("%s: got PMI2_IOC_INFO_GETATTR value %s valuelen %d\n", __func__, info_getjobattr_arg->value, info_getjobattr_arg->valuelen);
		    hcq_cmd_return (hcq, cmd, ret, 32*sizeof(char) + sizeof(int) + sizeof(int) + info_getjobattr_arg->valuelen,  info_getjobattr_arg);
		    break;
	    case PMI_IOC_ALLGATHER:
	      outbuf = (char *) malloc (job_size * data_len);
	      assert (outbuf);
	      printf("%s: got PMI2_IOC_ALLGATHER in %p out %p len %d\n", __func__, data_buf, outbuf, data_len);
	      allgather (data_buf, outbuf, data_len);
	      hcq_cmd_return (hcq, cmd, ret, (job_size * data_len), outbuf);
	      break;
	    case PMI_IOC_ALLGATHERV:
// TODO(npe): fix memory leak.
		    outbuf = (char *) malloc (sizeof(int) * job_size + job_size * data_len);
		    assert (outbuf);
		    printf("%s: got PMI2_IOC_ALLGATHERV in %p out %p len %d\n", __func__, data_buf, outbuf, data_len);
	            allgatherv (data_buf, data_len, outbuf + sizeof(int)*job_size, outbuf);
	            hcq_cmd_return (hcq, cmd, ret, (job_size * data_len), outbuf);
	            break;

	    case PMI_IOC_GETRANK:
	      rc = PMI_Get_rank (&my_rank);
	      hcq_cmd_return (hcq, cmd, ret, 4, &my_rank);
	      break;
	    case PMI_IOC_GETSIZE:
	      rc = PMI_Get_size (&job_size);
	      hcq_cmd_return (hcq, cmd, ret, 4, &job_size);
	      break;
	    case PMI_IOC_FINALIZE:
	      PMI_Finalize ();
	      hcq_cmd_return (hcq, cmd, ret, 0, NULL);
	      break;
	    case PMI_IOC_MALLOC:

	      mallocsz = (int *) data_buf;
	      rc = posix_memalign ((void **) &buffer, 4096, *mallocsz);
	      assert (rc == 0);
/*
	      fprintf (stderr, "server malloc return buffer size %p  %d\n",
		       buffer, *mallocsz);
*/
	      size_t msz = (*mallocsz);
	      reg_mem_seg = xemem_make (buffer, msz, NULL);
	      hcq_cmd_return (hcq, cmd, ret, sizeof (uint64_t), &reg_mem_seg);
	      list_add_element (mt, (void*)&reg_mem_seg, (uint64_t)buffer, msz);
	      break;

	   case OOB_IOC_PTAG:
		   printf("%s: returning ptag %d\n", __func__, ptag);
		   hcq_cmd_return (hcq, cmd, ret, 4, &ptag);
		      break;
	    case OOB_IOC_COOKIE:
		    printf("%s: returning cookie %d\n", __func__, ptag);
		    hcq_cmd_return (hcq, cmd, ret, 4, &cookie);
		    break;
	    case OOB_IOC_DEV_ID:
		    printf("%s: returning devid %d\n", __func__, devid);
		    hcq_cmd_return (hcq, cmd, ret, 4, &devid);
		    break;
	    case OOB_IOC_LOC_ADDR:
		    printf("%s: returning local addr %s\n", __func__, local_addr);
		    hcq_cmd_return (hcq, cmd, ret, strlen(local_addr), local_addr);
		    break;
	    default:
		    printf("%s: GOT UNKNOWN COMMAND CODE\n", __func__);
	      break;
	    }
	}

    }
  close (device);

  printf ("Deviced is closed\n");
  hcq_free_queue (hcq);
  hobbes_client_deinit ();

  return 0;

}
int
setup_ugni(void)
{
    int alps_status = 0, num_creds, i, len;
    uint64_t apid;
    size_t alps_count;
    int ret = 0;
    alpsAppLLIGni_t *rdmacred_rsp=NULL;
    alpsAppGni_t *rdmacred_buf;
    char *ptr;
    char env_buffer[1024];
    static int already_got_creds = 0;
    printf("%s: setting up ugni\n", __func__);
    /*
     * If we already put the GNI RDMA credentials into orte_launch_environ,
     * no need to do anything.
     * TODO: kind of ugly, need to implement an opal_getenv
     */

    if (1 == already_got_creds) {
        return 0;
    }

    /*
     * get the Cray HSN RDMA credentials here and stuff them in to the
     * PMI env variable format expected by uGNI consumers like the uGNI
     * BTL, etc. Stuff into the orte_launch_environ to make sure the
     * application processes can actually use the HSN API (uGNI).
     */

//    if (OMPI_PROC_IS_DAEMON) {

        ret = alps_app_lli_lock();

        /*
         * First get our apid
         */

        ret = alps_app_lli_put_request(ALPS_APP_LLI_ALPS_REQ_APID, NULL, 0);
        if (ALPS_APP_LLI_ALPS_STAT_OK != ret) {
		printf("alps_app_lli_put_request returned %d",ret);
            //ret = OMPI_ERR_FILE_WRITE_FAILURE;
	    ret = -1;
            goto fn_exit;
        }

        ret = alps_app_lli_get_response (&alps_status, &alps_count);
        if (ALPS_APP_LLI_ALPS_STAT_OK != alps_status) {
		printf("alps_app_lli_get_response returned %d", alps_status);
            //ret = OMPI_ERR_FILE_READ_FAILURE;
	    ret = -1;
            goto fn_exit;
        }

        ret = alps_app_lli_get_response_bytes (&apid, sizeof(apid));
        if (ALPS_APP_LLI_ALPS_STAT_OK != ret) {
		printf("alps_app_lli_get_response_bytes returned %d", ret);
            //ret = OMPI_ERR_FILE_READ_FAILURE;
	    ret = -1;
            goto fn_exit;
        }

        /*
         * now get the GNI rdma credentials info
         */

        ret = alps_app_lli_put_request(ALPS_APP_LLI_ALPS_REQ_GNI, NULL, 0);
        if (ALPS_APP_LLI_ALPS_STAT_OK != ret) {
		printf("alps_app_lli_put_request returned %d",ret);
            //ret = OMPI_ERR_FILE_WRITE_FAILURE;
	    ret = -1;
            goto fn_exit;
        }

        ret = alps_app_lli_get_response(&alps_status, &alps_count);
        if (ALPS_APP_LLI_ALPS_STAT_OK != alps_status) {
		printf("alps_app_lli_get_response returned %d", alps_status);
            //ret = OMPI_ERR_FILE_READ_FAILURE;
	    ret = -1;
            goto fn_exit;
        }

        rdmacred_rsp = (alpsAppLLIGni_t *)malloc(alps_count);
        if (NULL == rdmacred_rsp) {
		//ret = OMPI_ERR_OUT_OF_RESOURCE;
		ret = -1;
            goto fn_exit;
        }

        memset(rdmacred_rsp,0,alps_count);

        ret = alps_app_lli_get_response_bytes(rdmacred_rsp, alps_count);
        if (ALPS_APP_LLI_ALPS_STAT_OK != ret) {
		printf("alps_app_lli_get_response_bytes returned %d",ret);
            free(rdmacred_rsp);
            // ret = OMPI_ERR_FILE_READ_FAILURE;
	    ret = -1;
            goto fn_exit;
        }

        ret = alps_app_lli_unlock();

        rdmacred_buf = (alpsAppGni_t *)(rdmacred_rsp->u.buf);

        /*
         * now set up the env. variables -
         * The cray pmi sets up 4 environment variables:
         * PMI_GNI_DEV_ID - format (id0:id1....idX)
         * PMI_GNI_LOC_ADDR - format (locaddr0:locaddr1:....locaddrX)
         * PMI_GNI_COOKIE - format (cookie0:cookie1:...cookieX)
         * PMI_GNI_PTAG - format (ptag0:ptag1:....ptagX)
         *
         * where X == num_creds - 1
         *
         * TODO: need in theory to check for possible overrun of env_buffer
         */

        num_creds = rdmacred_rsp->count;

        /*
         * first build ptag env
         */

        memset(env_buffer,0,sizeof(env_buffer));
        ptr = env_buffer;
        for (i=0; i<num_creds-1; i++) {
            len = sprintf(ptr,"%d:",rdmacred_buf[i].ptag);
            ptr += len;
        }
	ptag = rdmacred_buf[i].ptag;
        sprintf(ptr,"%d",rdmacred_buf[num_creds-1].ptag);
        ret = setenv("PMI_GNI_PTAG", env_buffer, false);
        if (ret != 0) {
		printf("setenv for PMI_GNI_TAG returned %d", ret);
            goto fn_exit;
        }
    printf("%s: set GNI PTAG to %s\n", __func__, getenv("PMI_GNI_PTAG"));
    printf("%s: set ptag to %d\n", __func__, ptag);
        /*
         * the cookie env
         */

        memset(env_buffer,0,sizeof(env_buffer));
        ptr = env_buffer;
        for (i=0; i<num_creds-1; i++) {
            len = sprintf(ptr,"%d:",rdmacred_buf[i].cookie);
            ptr += len;
        }
	cookie = rdmacred_buf[num_creds-1].cookie;
        sprintf(ptr,"%d",rdmacred_buf[num_creds-1].cookie);
        ret = setenv("PMI_GNI_COOKIE", env_buffer, false);
printf("%s: set GNI COOKIE to %s\n", __func__, getenv("PMI_GNI_COOKIE"));
    printf("%s: set cookie to %d\n", __func__, cookie);

        if (ret != 0) {
		printf("setenv for PMI_GNI_COOKIE returned %d", ret);
            goto fn_exit;
        }

        /*
         * nic loc addrs
         */

        memset(env_buffer,0,sizeof(env_buffer));
        ptr = env_buffer;
        for (i=0; i<num_creds-1; i++) {
            len = sprintf(ptr,"%d:",rdmacred_buf[i].local_addr);
           ptr += len;
        }
        sprintf(ptr,"%d",rdmacred_buf[num_creds-1].local_addr);
printf("%s: PMI_GNI_LOC_ADDR %s\n", __func__, ptr);
local_addr = strdup(ptr);
        ret = setenv("PMI_GNI_LOC_ADDR", env_buffer, false);
        if (ret != 0) {
		printf("setenv for PMI_GNI_LOC_ADDR returned %d\n", ret);
            goto fn_exit;
        }

        /*
         * finally device ids
         */

        memset(env_buffer,0,sizeof(env_buffer));
        ptr = env_buffer;
        for (i=0; i<num_creds-1; i++) {
            len = sprintf(ptr,"%d:",rdmacred_buf[i].device_id);
            ptr += len;
        }
        devid = rdmacred_buf[i].device_id;
        printf("%s: rdmacred_buf[i].device_id %d\n", __func__, rdmacred_buf[i].device_id);
        sprintf(ptr,"%d",rdmacred_buf[num_creds-1].device_id);
printf("%s: PMI_GNI_DEV_ID %s\n", __func__, ptr);
        ret = setenv("PMI_GNI_DEV_ID", env_buffer, false);
        if (ret != 0) {
		printf("opal_setenv for PMI_GNI_DEV_ID returned %d", ret);
            goto fn_exit;
        }

	//   }
{
	gni_job_res_desc_t res_des;
        ret = GNI_GetJobResInfo (devid, ptag,
                                 GNI_JOB_RES_MDD, &res_des);
printf("%s: GNI_GetJobResInfo rc %d res_des %p\n", __func__, ret, res_des);
}
   fn_exit:
    if (0 == ret) already_got_creds = 1;
    return ret;
}
