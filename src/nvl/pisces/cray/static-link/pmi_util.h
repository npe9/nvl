#ifndef _PMI_UTIL_H_
#define _PMI_UTIL_H_

#include <linux/ioctl.h>
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __KERNEL__
    #include <sys/ioctl.h>
#else
    #include <linux/ioctl.h>
#endif


typedef struct pmi_allgather_args {
        uint32_t        comm_size;         /* IN size of world_comm */
        void            *in_data;          /* IN data to post for gather*/
        uint16_t        in_data_len;       /* IN size of the data to post */
        void*         out_data;       /* OUT  after gather from all ranks */
} pmi_allgather_args_t;

typedef struct pmi_allgatherv_args {
        uint32_t        comm_size;         /* IN size of world_comm */
        void            *in_data;          /* IN data to post for gather*/
        uint16_t        in_data_len;       /* IN size of the data to post */
        void*         out_data;       /* OUT  after gather from all ranks */
	int *all_len;
} pmi_allgatherv_args_t;

typedef struct pmi_getsize_args {
        uint32_t        comm_size;         /* IN size of world_comm */
} pmi_getsize_args_t;

typedef struct pmi_getrank_args {
        uint32_t        myrank;         /* IN size of world_comm */
} pmi_getrank_args_t;

typedef struct pmi2_init_args {
	uint32_t spawned;
	uint32_t size;
        uint32_t rank;         /* IN size of world_comm */
	uint32_t appnum;
} pmi2_init_args_t;

typedef struct pmi2_job_getid_args {
	uint32_t jobid_size;
	char jobid[];
} pmi2_job_getid_args_t;

typedef struct pmi2_info_getjobattr_args {
	char name[32];
	uint32_t found;
        uint32_t valuelen;
        char value[];
} pmi2_info_getjobattr_args_t;

typedef struct gni_getjobresinfo_args {
	uint32_t            device_id;
	uint8_t             ptag;
	gni_job_res_t       res_type;
	gni_job_res_desc_t  *res_desc
} gni_getjobresinfo_args_t;

/* IOCTL commands */
#define PMI_IOC_MAGIC   'P'
#define PMI_IOC_ALLGATHER     _IOWR(PMI_IOC_MAGIC, 1, pmi_allgather_args_t) 
#define PMI_IOC_GETSIZE        _IOWR(PMI_IOC_MAGIC, 2, pmi_getsize_args_t) 
#define PMI_IOC_GETRANK        _IOWR(PMI_IOC_MAGIC, 3, pmi_getrank_args_t)
#define PMI_IOC_FINALIZE       _IOWR(PMI_IOC_MAGIC, 4,NULL) 
#define PMI_IOC_BARRIER        _IOWR(PMI_IOC_MAGIC, 5, NULL)
#define PMI_IOC_MALLOC        _IOWR(PMI_IOC_MAGIC, 6, NULL)
#define PMI_IOC_INIT          _IOWR(PMI_IOC_MAGIC, 7, int)
#define PMI_IOC_ALLGATHERV     _IOWR(PMI_IOC_MAGIC, 8, pmi_allgatherv_args_t) 

#define PMI2_IOC_MAGIC   'Q'
#define PMI2_IOC_INIT          _IOWR(PMI2_IOC_MAGIC, 1, pmi2_init_args_t)
#define PMI2_IOC_JOB_GETID     _IOWR(PMI2_IOC_MAGIC, 2, pmi2_job_getid_args_t)
#define PMI2_IOC_INFO_GETJOBATTR _IOWR(PMI2_IOC_MAGIC, 3, pmi2_info_getjobattr_args_t)

#define OOB_IOC_MAGIC 'Z'
#define OOB_IOC_PTAG _IOWR(OOB_IOC_MAGIC, 1, int)
#define OOB_IOC_COOKIE _IOWR(OOB_IOC_MAGIC, 2, int)
#define OOB_IOC_DEV_ID _IOWR(OOB_IOC_MAGIC, 3, int)
#define OOB_IOC_LOC_ADDR _IOWR(OOB_IOC_MAGIC, 4, char*)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*_PMI_UTIL_H_*/

