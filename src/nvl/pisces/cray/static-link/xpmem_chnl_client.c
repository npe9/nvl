/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

/* User level test procedures */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
//#include <sys/fcntl.h>

#include "gni_pub.h"
#include "gni_priv.h"
#include <alps/libalpslli.h>
#include <sys/utsname.h>

#include "hobbes_cmd_queue.h"
#include "xemem.h"
#include "xpmem.h"
#include <hobbes_cmd_queue.h>
#include <xemem.h>
#include <hobbes_enclave.h>
#include <dlfcn.h>		/* For dlsym */
#include "pmi.h"		/* Client should have just include pmi.h but not link libpmi, server might */
#include <assert.h>		/* Client should have just include pmi.h but not link libpmi, server might */
#include "xmem_list.h"
#include "pmi_util.h"

/*  For utilities of PMI etc */
int my_rank, comm_size;

struct utsname uts_info;

/*  This is to implement utility functions
 * allgather gather the requested information from all of the ranks.
 */

int client_fd, pmi_fd;
xemem_segid_t cmdq_segid;
hcq_handle_t hcq = HCQ_INVALID_HANDLE;
struct memseg_list *mt = NULL;

int handle_ioctl (int device, unsigned long int request, void *arg);

void
allgather (void *in, void *out, int len)
{
	uint32_t retlen;
	void *out_ptr;
	typedef struct {
		uint32_t pmix_rank;
		int32_t jobfam;
		int32_t vpid;
		int32_t nbytes;
	} bytes_and_rank_t;

	int i;
	bytes_and_rank_t *p;

	hcq_cmd_t cmd = HCQ_INVALID_CMD;
	//fprintf(stdout, "client library allgather in pointer  %p, \n", in);
	comm_size = 2;
	cmd = hcq_cmd_issue (hcq, PMI_IOC_ALLGATHER, len, in);
	out_ptr = hcq_get_ret_data (hcq, cmd, &retlen);
	printf("%s: comm_size %d len %d comm_size * len %d out_ptr %p\n", __func__, comm_size, len, comm_size*len, out_ptr);
	p = out_ptr;
	for (i = 0; i < 2; i++)
	{
		printf("%s: pmix_rank %d vpid %d jobfam %d nbytes %d\n", __func__, p[i].pmix_rank, p[i].vpid, p[i].jobfam, p[i].nbytes);
//      memcpy (&out_ptr[len * ivec_ptr[i]], &tmp_buf[i * len], len);
	}

	memcpy (out, out_ptr, (comm_size * len));
	hcq_cmd_complete (hcq, cmd);
}

int
allgatherv (void *in, int len, void *out, int *all_lens)
{
	uint32_t retlen;
	void *out_ptr;
	typedef struct {
		uint32_t pmix_rank;
		int32_t jobfam;
		int32_t vpid;
		int32_t nbytes;
	} bytes_and_rank_t;

	int i;
	bytes_and_rank_t *p;

	hcq_cmd_t cmd = HCQ_INVALID_CMD;
	//fprintf(stdout, "client library allgather in pointer  %p, \n", in);
	comm_size = 2;
	cmd = hcq_cmd_issue (hcq, PMI_IOC_ALLGATHERV, len, in);
	out_ptr = hcq_get_ret_data (hcq, cmd, &retlen);
	printf("%s: comm_size %d len %d comm_size * len %d out_ptr %p\n", __func__, comm_size, len, comm_size*len, out_ptr);
	all_lens = out_ptr;
	p = out_ptr + 2*sizeof(int);
	for (i = 0; i < 2; i++)
	{
		printf("%s: pmix_rank %d vpid %d jobfam %d nbytes %d\n", __func__, p[i].pmix_rank, p[i].vpid, p[i].jobfam, p[i].nbytes);
//      memcpy (&out_ptr[len * ivec_ptr[i]], &tmp_buf[i * len], len);
	}

	memcpy (out, out_ptr, (comm_size * len));
	hcq_cmd_complete (hcq, cmd);
	return 0;
}


/* Define bare minimum PMI calls to forward to other side */

int
pmi_finalize (void)
{
  //int retlen;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  cmd = hcq_cmd_issue (hcq, PMI_IOC_FINALIZE, 0, NULL);
  hcq_cmd_complete (hcq, cmd);
  return PMI_SUCCESS;
}

int
pmi_barrier (void)
{
  //int retlen;
	printf("%s: entered\n", __func__);
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  cmd = hcq_cmd_issue (hcq, PMI_IOC_BARRIER, 0, NULL);
  hcq_cmd_complete (hcq, cmd);
  return PMI_SUCCESS;
}

/*  End of For utilities of PMI etc */

#define MAXIMUM_CQ_RETRY_COUNT 500

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

#define FMA_WINDOW_SIZE    (1024 * 1024 * 1024L)
#define GHAL_FMA_CTRL_SIZE      0x1000UL

int __real_open (const char *pathname, int flags);

extern char **environ;

int
kgni_init (const char *pathname, int flags)
{
  static int already_init = 0;
  char *buf;
  int *ptag, *cookie, *devid;
  char *local_addr;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  uint32_t len;
  if (!already_init)
    {
      mt = list_new ();
      hobbes_client_init ();
      cmdq_segid = xemem_lookup_segid ("GEMINI-NEW");

      hcq = hcq_connect (cmdq_segid);
      if (hcq == HCQ_INVALID_HANDLE)
	{
	  printf ("connect failed\n");
	  return -1;
	}
      cmd = hcq_cmd_issue (hcq, OOB_IOC_PTAG, 0, NULL);
      ptag = (int*)hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      asprintf(&buf, "PMI_GNI_PTAG=%d", *ptag);
      printf("%s: putting %s in environment\n", __func__, buf);
      putenv(buf);
      printf("%s: got %s from environment\n", __func__, getenv("PMI_GNI_PTAG"));
      {
	      char **s;
	      s = environ;
	      while(*s)
		      printf("%s: %s\n", __func__, *s++);
      }
      cmd = hcq_cmd_issue (hcq, OOB_IOC_COOKIE, 0, NULL);
      cookie = (int*)hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      asprintf(&buf, "PMI_GNI_COOKIE=%d", *cookie);
      printf("%s: putting %s in environment\n", __func__, buf);
      putenv(buf);
      printf("%s: got %s from environment\n", __func__, getenv("PMI_GNI_COOKIE"));
      cmd = hcq_cmd_issue (hcq, OOB_IOC_DEV_ID, 0, NULL);
      devid = (int*)hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      asprintf(&buf, "PMI_GNI_DEV_ID=%d", *devid);
      printf("%s: putting %s in environment\n", __func__, buf);
      putenv(buf);
      printf("%s: got %s from environment\n", __func__, getenv("PMI_GNI_DEV_ID"));
      cmd = hcq_cmd_issue (hcq, OOB_IOC_LOC_ADDR, 0, NULL);
      local_addr = (char*)hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      asprintf(&buf, "PMI_GNI_LOC_ADDR=%s", local_addr);
      printf("%s: putting %s in environment\n", __func__, buf);
      putenv(buf);
      printf("%s: got %s from environment\n", __func__, getenv("PMI_GNI_LOC_ADDR"));

      {
      char **s;
      s = environ;
      while(*s)
	      printf("%s: %s\n", __func__, *s++);
      }
      already_init = 1;
    }
  return 0;
}

/* We need to start cmd queue for PMI calls or kgni ioctls */

int
__wrap_open (const char *pathname, int flags, ...)
{
  /* Some evil injected code goes here. */
  if (strncmp (pathname, "/dev/kgni0", 10) == 0)
    {
      //client_fd = __real_open ("/home/smukher/temp", flags);
      client_fd = 120;
      if(kgni_init (pathname, flags) < 0)
        return -1;
      return client_fd;
    }
  else if (strncmp (pathname, "/dev/pmi", 8) == 0)
    {
      //pmi_fd = __real_open ("/home/smukher/temp1", flags);
      pmi_fd = 121;
      if(kgni_init (pathname, flags) < 0)
        return -1;
      return pmi_fd;
    }
  else
    {
      return __real_open (pathname, flags);
    }
}

int __real_ioctl (int fd, int request, void *data);

int
__wrap_ioctl (int fd, unsigned long int request, ...)
{
  //char *msg;
  va_list args;
  void *argp;
  /* extract argp from varargs */
  va_start (args, request);
  argp = va_arg (args, void *);
  va_end (args);

  if (fd == client_fd)
    {
      //fprintf (stdout, "ioctl: on kgni device\n");
      //fflush (stdout);
      handle_ioctl (client_fd, request, argp);
      return 0;
    }
  else if (fd == pmi_fd)
    {
      //fprintf (stdout, "ioctl: on PMI device\n");
      //fflush (stdout);
      handle_ioctl (client_fd, request, argp);
      return 0;
    }
  else
    {
      return __real_ioctl (fd, request, argp);
    }
}


int
pack_args (unsigned long int request, void *args)
{
  gni_nic_setattr_args_t *nic_set_attr1;
  gni_mem_register_args_t *mem_reg_attr;
  //gni_mem_register_args_t *mem_reg_attr1;
  gni_cq_destroy_args_t *cq_destroy_args;
  gni_cq_create_args_t *cq_create_args;
  gni_cq_wait_event_args_t *cq_wait_event_args;
  gni_post_rdma_args_t *post_rdma_args;
  gni_nic_fmaconfig_args_t *fmaconfig_args;
  //void *reg_buf;
  void *rdma_post_tmpbuf;
  void *cq_tmpbuf;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  //gni_mem_segment_t *segment;	/*comes from gni_pub.h */
  //int i;
  //uint32_t seg_cnt;
  xemem_segid_t cq_index_seg;
  xemem_segid_t reg_mem_seg;
  xemem_segid_t my_mem_seg;
  xemem_segid_t cq_seg;
  xemem_segid_t rdma_post_seg;
  uint32_t len = 0;
  uint64_t save_local_addr;
  //int *rc;

  /* PMI ioctls args */
/* XXX(npe) put the args in a discriminated union */
  pmi_allgather_args_t *gather_arg;
  pmi_getsize_args_t *size_arg;
  pmi_getrank_args_t *rank_arg;
  pmi2_init_args_t *init_arg;
  pmi2_job_getid_args_t *job_getid_arg;
  pmi2_info_getjobattr_args_t *info_getjobattr_arg;
  int *size = malloc (sizeof (int));
  int *rank = malloc (sizeof (int));
  mdh_addr_t *clnt_gather_hdl;
  
  switch (request)
    {
    case GNI_IOC_NIC_SETATTR:
      /* IOCTL  1. handle NIC_SETATTR ioctl for client */
      nic_set_attr1 = (gni_nic_setattr_args_t *) args;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_NIC_SETATTR,
		       sizeof (gni_nic_setattr_args_t), nic_set_attr1);
      fprintf (stdout, "cmd = %0lx data size %lu\n", cmd,
	       sizeof (nic_set_attr1));
      nic_set_attr1 = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) nic_set_attr1, sizeof (nic_set_attr1));
      printf ("after NIC attach  dump segids %lu  %lu %lu %lu\n",
	      nic_set_attr1->fma_window, nic_set_attr1->fma_window_nwc,
	      nic_set_attr1->fma_window_get, nic_set_attr1->fma_ctrl);
      struct xemem_addr win_addr;
      struct xemem_addr put_addr;
      struct xemem_addr get_addr;
      struct xemem_addr ctrl_addr;


      void *fma_win = NULL;
      void *fma_put = NULL;
      void *fma_get = NULL;
      void *fma_ctrl = NULL;
      //void *fma_nc = NULL;

      xemem_apid_t apid;

      //xemem_segid_t fma_win_seg;
      //xemem_segid_t fma_put_seg;
      //xemem_segid_t fma_get_seg;
      //xemem_segid_t fma_ctrl_seg;
      hcq_cmd_t cmd = HCQ_INVALID_CMD;


      apid = xemem_get (nic_set_attr1->fma_window, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (nic_set_attr1->fma_window);
	  return -1;
	}

      win_addr.apid = apid;
      win_addr.offset = 0;

      fma_win = xemem_attach_nocache (win_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_win == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (nic_set_attr1->fma_window);
	  return -1;
	}

      apid = xemem_get (nic_set_attr1->fma_window_nwc, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (nic_set_attr1->fma_window_nwc);
	  return -1;		//invalid handle
	}

      put_addr.apid = apid;
      put_addr.offset = 0;

      fma_put = xemem_attach_nocache (put_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_put == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (nic_set_attr1->fma_window_nwc);
	  return -1;		// invalid HCQ handle
	}
/***************************/
      apid = xemem_get (nic_set_attr1->fma_window_get, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (nic_set_attr1->fma_window_get);
	  return -1;
	}

      get_addr.apid = apid;
      get_addr.offset = 0;

      fma_get = xemem_attach_nocache (get_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_get == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (nic_set_attr1->fma_window_get);
	  return -1;
	}
      apid = xemem_get (nic_set_attr1->fma_ctrl, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (nic_set_attr1->fma_ctrl);
	  return -1;
	}

      ctrl_addr.apid = apid;
      ctrl_addr.offset = 0;

      fma_ctrl = xemem_attach_nocache (ctrl_addr, GHAL_FMA_CTRL_SIZE, NULL);

      if (fma_ctrl == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (nic_set_attr1->fma_ctrl);
	  return -1;
	}

      nic_set_attr1->fma_window = (uint64_t)fma_win;
      nic_set_attr1->fma_window_nwc = (uint64_t)fma_put;
      nic_set_attr1->fma_window_get = (uint64_t)fma_get;
      nic_set_attr1->fma_ctrl = (uint64_t)fma_ctrl;
      memcpy (args, (void *) nic_set_attr1, sizeof (gni_nic_setattr_args_t));
      break;
    case GNI_IOC_CQ_CREATE:
      cq_create_args =
	(gni_cq_create_args_t *) malloc (sizeof (gni_cq_create_args_t));
      memcpy (cq_create_args, args, sizeof (gni_cq_create_args_t));
      /* Following is what we get in args for this ioctl from client side 
         cq_create_args.queue = (gni_cq_entry_t *) cq->queue;
         cq_create_args.entry_count = entry_count;
         cq_create_args.mem_hndl = cq->mem_hndl;
         cq_create_args.mode = mode;
         cq_create_args.delay_count = delay_count;
         cq_create_args.kern_cq_descr = GNI_INVALID_CQ_DESCR;
         cq_create_args.interrupt_mask = NULL;
         fprintf (stdout, "client ioctl for cq create %p, entry count %d\n",
         cq_create_args->queue, cq_create_args->entry_count);
         fprintf (stdout, " cLIENT CQ_CREATE mem hndl word1=0x%16lx, word2 = 0x%16lx\n",
         cq_create_args->mem_hndl.qword1, cq_create_args->mem_hndl.qword2);
       */
      cq_tmpbuf = cq_create_args->queue;	/* preserve cq space */
      cq_seg = list_find_segid_by_vaddr (mt, (uint64_t)cq_create_args->queue);
      cq_create_args->queue = (gni_cq_entry_t *) cq_seg;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_CQ_CREATE, sizeof (gni_cq_create_args_t),
		       cq_create_args);
      cq_create_args = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      cq_create_args->queue = (gni_cq_entry_t *) cq_tmpbuf;	/* restore cq space */
      memcpy (args, (void *) cq_create_args, sizeof (gni_cq_create_args_t));
      break;

    case GNI_IOC_MEM_REGISTER:
      /* If memory is segmented we just loop through the list 
         segments[0].address && segments[0].length
       */
      mem_reg_attr =
	(gni_mem_register_args_t *) malloc (sizeof (gni_mem_register_args_t));
      memcpy (mem_reg_attr, args, sizeof (gni_mem_register_args_t));

      reg_mem_seg =
	xemem_make ((void*)mem_reg_attr->address, mem_reg_attr->length, NULL);
      list_add_element (mt, &reg_mem_seg, mem_reg_attr->address,
			mem_reg_attr->length);
      //list_print(mt);
      mem_reg_attr->address = (uint64_t) reg_mem_seg;
      // Now segid actually is 64 bit so push it there
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_MEM_REGISTER,
		       sizeof (gni_mem_register_args_t),
		       (char *) mem_reg_attr);

      mem_reg_attr = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) mem_reg_attr, sizeof (gni_mem_register_args_t));
      break;

    case GNI_IOC_CQ_WAIT_EVENT:
      /*  cq_hndl->queue is a user /client address space area. i.e 
         During cq_create iocti, one needs to create a xemem seg of 4KB size
         cq_idx = GNII_CQ_REMAP_INDEX(cq_hndl, cq_hndl->read_index);

         cq_wait_event_args.kern_cq_descr =
         (uint64_t *)cq_hndl->kern_cq_descr;
         cq_wait_event_args.next_event_ptr =
         (uint64_t **)&cq_hndl->queue[cq_idx];
         cq_wait_event_args.num_cqs = 0;
       */
      cq_wait_event_args = (gni_cq_wait_event_args_t *) args;
      cq_index_seg =
	list_find_segid_by_vaddr (mt, (uint64_t)cq_wait_event_args->next_event_ptr);
      cq_wait_event_args->next_event_ptr = (void*)cq_index_seg;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_CQ_WAIT_EVENT,
		       sizeof (gni_cq_wait_event_args_t), cq_wait_event_args);
      cq_wait_event_args = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) cq_wait_event_args, sizeof (cq_wait_event_args));
      break;
    case GNI_IOC_CQ_DESTROY:
      cq_destroy_args = (gni_cq_destroy_args_t *) args;	/* ONLY need cq_destroy_args.kern_cq_descr, a opaque pointer */
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_CQ_DESTROY,
		       sizeof (gni_cq_destroy_args_t), cq_destroy_args);
      cq_destroy_args = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) cq_destroy_args, sizeof (cq_destroy_args));
      break;
    case PMI_IOC_ALLGATHER:
      gather_arg = (pmi_allgather_args_t *) args;	// Now check to see if we are gathering mem hnlds
      if (gather_arg->in_data_len == sizeof (mdh_addr_t))
	{
	  clnt_gather_hdl = gather_arg->in_data;
	  /*
	     fprintf(stdout, "Client casting :  0x%lx    0x%016lx    0x%016lx\n",
	     clnt_gather_hdl->addr,
	     clnt_gather_hdl->mdh.qword1,
	     clnt_gather_hdl->mdh.qword2);
	   */
	  my_mem_seg = list_find_segid_by_vaddr (mt, clnt_gather_hdl->addr);
	  clnt_gather_hdl->addr = (uint64_t) my_mem_seg;
	  //fprintf(stdout, "client  gather after segid found :  %llu\n", clnt_gather_hdl->addr);
	  //list_print(mt);
	}
      allgather (gather_arg->in_data, gather_arg->out_data,
		 gather_arg->in_data_len);
      break;
    case PMI_IOC_GETSIZE:
      size_arg = args;
      cmd = hcq_cmd_issue (hcq, PMI_IOC_GETSIZE, sizeof (int), size);
      size = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      comm_size = *size;
      size_arg->comm_size = *size;	/* Store it in a global */
      break;
    case PMI_IOC_GETRANK:
      rank_arg = args;
      cmd = hcq_cmd_issue (hcq, PMI_IOC_GETRANK, sizeof (int), rank);
      rank = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      rank_arg->myrank = *rank;	/* Store it in a global */
      my_rank = *rank;
      break;
    case PMI_IOC_FINALIZE:
      pmi_finalize ();
      break;
    case PMI_IOC_BARRIER:
      pmi_barrier ();
      break;
    case PMI2_IOC_INIT:
	    printf("%s: sending pmi2_init\n", __func__);
	    init_arg = (pmi2_init_args_t*)args;
	    cmd = hcq_cmd_issue (hcq, PMI2_IOC_INIT, sizeof (pmi2_init_args_t), init_arg);
	    init_arg = hcq_get_ret_data (hcq, cmd, &len);
	    hcq_cmd_complete (hcq, cmd);
	    memcpy (args, (void *) init_arg, sizeof (pmi2_init_args_t));
	    printf("%s: spawned %d rank %d size %d appnum %d\n", __func__, init_arg->spawned, init_arg->rank, init_arg->size, init_arg->appnum);
	    //comm_size = *size;
	    //size_arg->comm_size = *size;	/* Store it in a global */
	    break;
    case PMI2_IOC_JOB_GETID:
	    job_getid_arg = (pmi2_job_getid_args_t*)args;
	    printf("%s: sending pmi2_job_getid jobid %p jobid_size %d\n", __func__, job_getid_arg->jobid, job_getid_arg->jobid_size);
	    cmd = hcq_cmd_issue (hcq, PMI2_IOC_JOB_GETID, job_getid_arg->jobid_size + sizeof(int), job_getid_arg);
	    job_getid_arg = hcq_get_ret_data (hcq, cmd, &len);
	    hcq_cmd_complete (hcq, cmd);
	    memcpy (args, (void *) job_getid_arg, job_getid_arg->jobid_size + sizeof (int));
	    printf("%s: jobid %s jobid_size %d\n", __func__, job_getid_arg->jobid, job_getid_arg->jobid_size);
	    break;
    case PMI2_IOC_INFO_GETJOBATTR:
	    info_getjobattr_arg = (pmi2_info_getjobattr_args_t*)args;
	    printf("%s: sending pmi2_info_getjobattr value %p valuelen %d\n", __func__, info_getjobattr_arg->value, info_getjobattr_arg->valuelen);
	    cmd = hcq_cmd_issue (hcq, PMI2_IOC_INFO_GETJOBATTR, 32*sizeof(char) + sizeof(int) + sizeof(int) + info_getjobattr_arg->valuelen, info_getjobattr_arg);
	    info_getjobattr_arg = hcq_get_ret_data (hcq, cmd, &len);
	    hcq_cmd_complete (hcq, cmd);
	    memcpy (args, (void *) info_getjobattr_arg, 32*sizeof(char) + sizeof(int) + sizeof(int) + info_getjobattr_arg->valuelen);
	    printf("%s: value %s valuelen %u found %d\n", __func__, info_getjobattr_arg->value, info_getjobattr_arg->valuelen, info_getjobattr_arg->found);
	    break;
    case GNI_IOC_NIC_FMA_CONFIG:
      fmaconfig_args = (gni_nic_fmaconfig_args_t *) args;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_NIC_FMA_CONFIG,
		       sizeof (gni_nic_fmaconfig_args_t), fmaconfig_args);
      fmaconfig_args = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) fmaconfig_args,
	      sizeof (gni_nic_fmaconfig_args_t));
      break;
    case GNI_IOC_EP_POSTDATA:
      fprintf (stdout, "EP POSTDATA client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_TEST:
      fprintf (stdout, "EP POSTDATA test client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_WAIT:
      fprintf (stdout, "EP POSTDATA WAIT client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_TERMINATE:
      fprintf (stdout, "EP POSTDATA TERMINATE client case\n");
      break;

    case GNI_IOC_MEM_DEREGISTER:
      fprintf (stdout, "mem deregister client case\n");
      break;
    case GNI_IOC_POST_RDMA:
      post_rdma_args = args;
    case GNI_IOC_GETJOBRESINFO:
	    break;
/*
      fprintf (stdout,
	       "client side POST RDMA  local addr 0x%lx    word1 0x%016lx   word2  0x%016lx\n",
	       post_rdma_args->post_desc->local_addr,
	       post_rdma_args->post_desc->local_mem_hndl.qword1,
	       post_rdma_args->post_desc->local_mem_hndl.qword2);
*/
      save_local_addr = post_rdma_args->post_desc->local_addr;
      rdma_post_seg =
	list_find_segid_by_vaddr (mt, post_rdma_args->post_desc->local_addr);
      post_rdma_args->post_desc->local_addr = (uint64_t) rdma_post_seg;
      // Get a big buffer and pack post desc and rdma-args
      //fprintf (stdout, "post RDMA  client segid %llu\n", rdma_post_seg);
      rdma_post_tmpbuf =
	malloc (sizeof (gni_post_rdma_args_t) +
		sizeof (gni_post_descriptor_t));
      memcpy ((void *) rdma_post_tmpbuf,
	      post_rdma_args->post_desc, sizeof (gni_post_descriptor_t));
      memcpy (rdma_post_tmpbuf + sizeof (gni_post_descriptor_t),
	      post_rdma_args, sizeof (gni_post_rdma_args_t));
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_POST_RDMA,
		       sizeof (gni_post_rdma_args_t) +
		       sizeof (gni_post_descriptor_t), rdma_post_tmpbuf);

      //rc = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      post_rdma_args->post_desc->local_addr = (uint64_t) save_local_addr;
      free (rdma_post_tmpbuf);
      break;

    case GNI_IOC_NIC_VMDH_CONFIG:
      fprintf (stdout, "VMDH config  client case\n");
      break;
    case GNI_IOC_NIC_NTT_CONFIG:
      fprintf (stdout, "NTT config client case\n");
      break;

    case GNI_IOC_NIC_JOBCONFIG:
      fprintf (stdout, "NIC jobconfig client case\n");
      break;

    case GNI_IOC_FMA_SET_PRIVMASK:
      fprintf (stdout, "FMA set PRIVMASK client case\n");
      break;

    case GNI_IOC_SUBSCRIBE_ERR:
      fprintf (stdout, "subscribe error client case\n");
      break;
    case GNI_IOC_RELEASE_ERR:
      fprintf (stdout, "Release error client case\n");
      break;
    case GNI_IOC_SET_ERR_MASK:
      fprintf (stdout, "Set Err Mask client case\n");
      break;
    case GNI_IOC_GET_ERR_EVENT:
      fprintf (stdout, "Get Err event client case\n");
      break;
    case GNI_IOC_WAIT_ERR_EVENTS:
      fprintf (stdout, "Wait err event client case\n");
      break;
    case GNI_IOC_SET_ERR_PTAG:
      fprintf (stdout, "EP POSTDATA client case\n");
      break;
    case GNI_IOC_CDM_BARR:
      fprintf (stdout, "CDM BARR client case\n");
      break;
    case GNI_IOC_NIC_NTTJOB_CONFIG:
      fprintf (stdout, "NTT JOBCONFIG client case\n");
      break;
    case GNI_IOC_DLA_SETATTR:
      fprintf (stdout, "DLA SETATTR client case\n");
      break;
    case GNI_IOC_VCE_ALLOC:
      fprintf (stdout, "VCE ALLOC client case\n");
      break;
    case GNI_IOC_VCE_FREE:
      fprintf (stdout, "VCE FREE client case\n");
      break;
    case GNI_IOC_VCE_CONFIG:
      fprintf (stdout, "VCE CONFIG client case\n");
      break;
    case GNI_IOC_FLBTE_SETATTR:
      fprintf (stdout, "FLBTE SETATTR client case\n");
      break;
    case GNI_IOC_FMA_ASSIGN:
      fprintf (stdout, "FMA ASSIGN  client case\n");
      break;
    case GNI_IOC_FMA_UMAP:
      fprintf (stdout, "fma unmap client case\n");
      break;
    case GNI_IOC_MEM_QUERY_HNDLS:
      fprintf (stdout, "mem query hndlclient case\n");
      break;
    default:
      fprintf (stdout,
	       "Called default case in client side for kgni ioctl %lu\n",
	       request);
      break;
    }
  return 0;
}

int unpack_args (unsigned long int request, void *args);

int
handle_ioctl (int device, unsigned long int request, void *arg)
{

  pack_args (request, arg);
  unpack_args (request, arg);
  return 0;
}

int
unpack_args (unsigned long int request, void *args)
{
  //gni_nic_setattr_args_t *nic_set_attr;
  //gni_cq_wait_event_args_t *cq_wait_event_args;
  pmi2_init_args_t *init_arg;
  switch (request)
    {
    case GNI_IOC_NIC_SETATTR:

      break;
    case GNI_IOC_NIC_FMA_CONFIG:
      break;
    case GNI_IOC_EP_POSTDATA:
      break;
    case GNI_IOC_EP_POSTDATA_TEST:
      fprintf (stdout, "unpack EP POSTDATA test client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_WAIT:
      fprintf (stdout, "unpack EP POSTDATA WAIT client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_TERMINATE:
      fprintf (stdout, "unpack EP POSTDATA TERMINATE client case\n");
      break;

    case GNI_IOC_MEM_DEREGISTER:
      fprintf (stdout, "unpack mem deregister client case\n");
      break;
    case GNI_IOC_POST_RDMA:
      break;

    case GNI_IOC_NIC_VMDH_CONFIG:
      fprintf (stdout, "unpack VMDH config  client case\n");
      break;
    case GNI_IOC_NIC_NTT_CONFIG:
      fprintf (stdout, "unpack NTT config client case\n");
      break;

    case GNI_IOC_NIC_JOBCONFIG:
      fprintf (stdout, "unpack NIC jobconfig client case\n");
      break;

    case GNI_IOC_FMA_SET_PRIVMASK:
      fprintf (stdout, "unpack FMA set PRIVMASK client case\n");
      break; 
      return 0;
    }
}
