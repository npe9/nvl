#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "pmi.h"
#include "pmi2.h"
#include "pmi_cray_ext.h"
#include "pmi_util.h"
#include "hobbes_db.h"
#include "xemem.h"

#define PMI_MAX_STRING_LEN 128

extern hdb_db_t hobbes_master_db;

/* Set PMI_initialized to 1 for singleton init but no process manager
 * to help.  Initialized to 2 for normal initialization.  Initialized
 * to values higher than 2 when singleton_init by a process manager.
 * All values higher than 1 invlove a PM in some way. */
typedef enum {
	PMI_UNINITIALIZED   = 0,
	NORMAL_INIT_WITH_PM = 2,
} PMIState;

/* PMI client state */
static PMIState PMI_initialized = PMI_UNINITIALIZED;
static int      PMI_size = -1;
static int      PMI_rank = -1;
static int      PMI_kvsname_max = 0;
static int      PMI_keylen_max = 0;
static int      PMI_vallen_max = 0;
//static int      PMI_debug = 0;
//static int      PMI_spawned = 0;

static int xemem_poll_fd;


int
PMI_Init(int *spawned)
{
	char *p;

	PMI_initialized = PMI_UNINITIALIZED;

	if ((p = getenv("PMI_SIZE")) != NULL) {
		PMI_size = atoi(p);
	} else {
		printf("Failed to get PMI_SIZE environment variable.\n");
		return PMI_FAIL;
	}

	if ((p = getenv("PMI_RANK")) != NULL) {
		PMI_rank = atoi(p);
	} else {
		printf("Failed to get PMI_RANK environment variable.\n");
		return PMI_FAIL;
	}

	if (PMI_KVS_Get_name_length_max(&PMI_kvsname_max) != PMI_SUCCESS)
		return PMI_FAIL;

	if (PMI_KVS_Get_key_length_max(&PMI_keylen_max) != PMI_SUCCESS)
		return PMI_FAIL;

	if (PMI_KVS_Get_value_length_max(&PMI_vallen_max) != PMI_SUCCESS)
		return PMI_FAIL;

	if (spawned)
		*spawned = PMI_FALSE;

	/* Create a signallable segid for each process which exists for the lifetime of the process */
	/* TODO: Do we need to release these segids when the job completes? */
	xemem_segid_t segid = xemem_make_signalled(NULL, 0, NULL, &xemem_poll_fd);
	if (segid == XEMEM_INVALID_SEGID)
		return PMI_FAIL;

	if (hdb_create_pmi_barrier(hobbes_master_db, 0 /* appid */, PMI_rank, PMI_size, segid))
		return PMI_FAIL;

	PMI_initialized = NORMAL_INIT_WITH_PM;

	return PMI_SUCCESS;
}


int
PMI_Initialized(int *initialized)
{
	*initialized = (PMI_initialized != 0);
	return PMI_SUCCESS;
}


int
PMI_Finalize(void)
{
	// TODO: detach from whitedb
	return PMI_SUCCESS;
}


int
PMI_Get_size(int *size)
{
	*size = PMI_size;
	return PMI_SUCCESS;
}


int
PMI_Get_rank(int *rank)
{
	*rank = PMI_rank;
	return PMI_SUCCESS;
}


int
PMI_Get_universe_size(int *size)
{
	*size = PMI_size;
	return PMI_SUCCESS;
}


//TODO: Make this real
int
PMI_Get_appnum(int *appnum)
{
	if (appnum)
		*appnum = 0;

	return PMI_SUCCESS;
}


//TODO: Ignore for now
int
PMI_Publish_name(const char service_name[], const char port[])
{
	return PMI_SUCCESS;
}


//TODO: Ignore for now
int
PMI_Unpublish_name(const char service_name[])
{
	return PMI_SUCCESS;
}


//TODO: Ignore for now
int
PMI_Lookup_name(const char service_name[], char port[])
{
	return PMI_SUCCESS;
}


int
PMI_Barrier(void)
{
	printf("Made it into barrier\n");

	int i, count;
	xemem_segid_t *segids;

	count = hdb_pmi_barrier_increment(hobbes_master_db, 0 /* appid */);

	printf("hdb_pmi_barrier_increment() returned: %d\n", count);

	if (count == PMI_size) {
		segids = hdb_pmi_barrier_retire(hobbes_master_db, 0 /* appid */, PMI_size);

		if (segids == NULL)
			return PMI_FAIL;

		// We were the last ones here, signal everybody
		for (i = 0; i < PMI_size; i++) {
			if (i == PMI_rank)
				continue;
			if (xemem_signal_segid(segids[i])) {
				free(segids);
				return PMI_FAIL;
			}
		}

		free(segids);
	} else {
		struct pollfd ufd = {xemem_poll_fd, POLLIN, 0};
		while (1) {
			if (poll(&ufd, 1, -1) == -1) {
				return PMI_FAIL;
			} else {
				xemem_ack(xemem_poll_fd);
				break;
			}
		}
	}

	printf("Exitting PMI_Barrier()\n");

	return PMI_SUCCESS;
}


int
PMI_Abort(int exit_code, const char error_msg[])
{
	printf("aborting job:\n%s\n", error_msg);
	exit(exit_code);
	return -1;
}


//The KVS space name may not be actively used (we may not need more than one)
int
PMI_KVS_Get_my_name(char kvsname[], int length)
{
	strcpy(kvsname, "place_holder_kvsname");
	return PMI_SUCCESS;
}


int
PMI_KVS_Get_name_length_max(int *length)
{
	if (length)
		*length = PMI_MAX_STRING_LEN;

	return PMI_SUCCESS;
}


int
PMI_Get_id_length_max(int *length)
{
	if (length)
		*length = PMI_MAX_STRING_LEN;

	return PMI_SUCCESS;
}


int
PMI_KVS_Get_key_length_max(int *length)
{
	if (length)
		*length = PMI_MAX_STRING_LEN;

	return PMI_SUCCESS;
}


int
PMI_KVS_Get_value_length_max(int *length)
{
	if (length)
		*length = PMI_MAX_STRING_LEN;

	return PMI_SUCCESS;
}


int
PMI_KVS_Put(
	const char kvsname[],
	const char key[],
	const char value[]
	)
{
	/* TODO: Use the real Hobbes AppID rather than '0' (second arg) */
	if (hdb_put_pmi_keyval(hobbes_master_db, 0, kvsname, key, value) != 0)
		return PMI_FAIL;

	return PMI_SUCCESS;
}


int
PMI_KVS_Commit(const char kvsname[])
{
	return PMI_SUCCESS;
}


int
PMI_KVS_Get(
	const char kvsname[],
	const char key[],
	char       value[],
	int        length
	)
{
	const char * db_val;

	if (hdb_get_pmi_keyval(hobbes_master_db, 0, kvsname, key, &db_val) != 0)
		return PMI_FAIL;

	strncpy(value, db_val, length);
	if (length > 0)
		value[length - 1] = '\0';

	return PMI_SUCCESS;
}


int
PMI_Spawn_multiple(int count,
		   const char * cmds[],
		   const char ** argvs[],
		   const int maxprocs[],
		   const int info_keyval_sizesp[],
		   const PMI_keyval_t * info_keyval_vectors[],
		   int preput_keyval_size,
		   const PMI_keyval_t preput_keyval_vector[],
		   int errors[])
{
	return PMI_FAIL;
}


/* TODO: Implement these */
int
PMI_Get_kvs_domain_id( char id_str[], int length )
{
	if(!id_str)
		return PMI_ERR_INVALID_ARG;

	id_str="0";

	return PMI_SUCCESS;
}


int
PMI_Get_clique_size( int *size)
{
	*size=PMI_size;
	return PMI_SUCCESS;
}


int
PMI_Get_clique_ranks(int ranks[], int length)
{
	int i;

	if(!ranks)
		return PMI_ERR_INVALID_ARG;

	for (i=0; i<PMI_size && i < length; i++)
	{
		ranks[i] = i;
	}

	if(length != PMI_size)
		return PMI_ERR_INVALID_LENGTH;

	return PMI_SUCCESS;
}

/*@
  PMI2_Init - initialize the Process Manager Interface

  Output Parameter:
  + spawned - spawned flag
  . size - number of processes in the job
  . rank - rank of this process in the job
  - appnum - which executable is this on the mpiexec commandline

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  Initialize PMI for this process group. The value of spawned indicates whether
  this process was created by 'PMI2_Spawn_multiple'.  'spawned' will be non-zero
  iff this process group has a parent.

  @*/


int fd;

int PMI2_Init(int *spawned, int *size, int *rank, int *appnum) {
	pmi2_init_args_t out;
	printf("%s: entered\n", __func__);
	fd = open("/dev/pmi", O_RDWR);
	ioctl(fd, PMI2_IOC_INIT, &out);
	*spawned = out.spawned;
	*size = out.size;
	*rank = out.rank;
	*appnum = out.appnum;
	printf("%s: exited\n", __func__);
	return PMI_SUCCESS;
}

/*@
  PMI2_Finalize - finalize the Process Manager Interface

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  Finalize PMI for this job.

  @*/
int PMI2_Finalize(void) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Initialized - check if PMI has been initialized

  Return values:
  Non-zero if PMI2_Initialize has been called successfully, zero otherwise.

  @*/
int PMI2_Initialized(void) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Abort - abort the process group associated with this process

  Input Parameters:
  + flag - non-zero if all processes in this job should abort, zero otherwise
  - error_msg - error message to be printed

  Return values:
  If the abort succeeds this function will not return.  Returns an MPI
  error code otherwise.

  @*/
int PMI2_Abort(int flag, const char msg[]) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Spawn - spawn a new set of processes

  Input Parameters:
  + count - count of commands
  . cmds - array of command strings
  . argvs - array of argv arrays for each command string
  . maxprocs - array of maximum processes to spawn for each command string
  . info_keyval_sizes - array giving the number of elements in each of the
  'info_keyval_vectors'
  . info_keyval_vectors - array of keyval vector arrays
  . preput_keyval_size - Number of elements in 'preput_keyval_vector'
  . preput_keyval_vector - array of keyvals to be pre-put in the spawned keyval space
  - jobIdSize - size of the buffer provided in jobId

  Output Parameter:
  + jobId - job id of the spawned processes
  - errors - array of errors for each command

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  This function spawns a set of processes into a new job.  The 'count'
  field refers to the size of the array parameters - 'cmd', 'argvs', 'maxprocs',
  'info_keyval_sizes' and 'info_keyval_vectors'.  The 'preput_keyval_size' refers
  to the size of the 'preput_keyval_vector' array.  The 'preput_keyval_vector'
  contains keyval pairs that will be put in the keyval space of the newly
  created job before the processes are started.  The 'maxprocs' array
  specifies the desired number of processes to create for each 'cmd' string.
  The actual number of processes may be less than the numbers specified in
  maxprocs.  The acceptable number of processes spawned may be controlled by
  ``soft'' keyvals in the info arrays.  The ``soft'' option is specified by
  mpiexec in the MPI-2 standard.  Environment variables may be passed to the
  spawned processes through PMI implementation specific 'info_keyval' parameters.
  @*/
int PMI2_Job_Spawn(int count, const char * cmds[], const char ** argvs[],
		   const int maxprocs[],
		   const int info_keyval_sizes[],
		   const struct MPID_Info *info_keyval_vectors[],
		   int preput_keyval_size,
		   const struct MPID_Info *preput_keyval_vector[],
		   char jobId[], int jobIdSize,
		   int errors[]) {
	printf("%s: entered\n", __func__);
	return 0;
}


/*@
  PMI2_Job_GetId - get job id of this job

  Input parameters:
  . jobid_size - size of buffer provided in jobid

  Output parameters:
  . jobid - the job id of this job

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  @*/
int PMI2_Job_GetId(char jobid[], int jobid_size) {
	printf("%s: entered\n", __func__);
	pmi2_job_getid_args_t *out;
	printf("%s: entered jobid %p jobid_size %d\n", __func__, (void*)jobid, jobid_size);
	fd = open("/dev/pmi", O_RDWR);
	out = malloc(sizeof(int) + jobid_size);
	out->jobid_size = jobid_size;
// TODO(npe) errorpath
	ioctl(fd, PMI2_IOC_JOB_GETID, out);
	memcpy(jobid, out->jobid, jobid_size);
	printf("%s: exited jobid %s jobid_size %d\n", __func__, jobid, jobid_size);
	return PMI_SUCCESS;
}

/*@
  PMI2_Job_Connect - connect to the parallel job with ID jobid

  Input parameters:
  . jobid - job id of the job to connect to

  Output parameters:
  . conn - connection structure used to exteblish communication with
  the remote job

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  This just "registers" the other parallel job as part of a parallel
  program, and is used in the PMI2_KVS_xxx routines (see below). This
  is not a collective call and establishes a connection between all
  processes that are connected to the calling processes (on the one
  side) and that are connected to the named jobId on the other
  side. Processes that are already connected may call this routine.

  @*/
int PMI2_Job_Connect(const char jobid[], PMI2_Connect_comm_t *conn) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Job_Disconnect - disconnects from the job with ID jobid

  Input parameters:
  . jobid - job id of the job to connect to

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  @*/
int PMI2_Job_Disconnect(const char jobid[]) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_KVS_Put - put a key/value pair in the keyval space for this job

  Input Parameters:
  + key - key
  - value - value

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  If multiple PMI2_KVS_Put calls are made with the same key between
  calls to PMI2_KVS_Fence, the behavior is undefined. That is, the
  value returned by PMI2_KVS_Get for that key after the PMI2_KVS_Fence
  is not defined.

  @*/
int PMI2_KVS_Put(const char key[], const char value[])
{
	printf("%s: entered\n", __func__);
	return 0;
}
/*@
  PMI2_KVS_Fence - commit all PMI2_KVS_Put calls made before this fence

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  This is a collective call across the job.  It has semantics that are
  similar to those for MPI_Win_fence and hence is most easily
  implemented as a barrier across all of the processes in the job.
  Specifically, all PMI2_KVS_Put operations performed by any process in
  the same job must be visible to all processes (by using PMI2_KVS_Get)
  after PMI2_KVS_Fence completes.  However, a PMI implementation could
  make this a lazy operation by not waiting for all processes to enter
  their corresponding PMI2_KVS_Fence until some process issues a
  PMI2_KVS_Get. This might be appropriate for some wide-area
  implementations.

  @*/
int PMI2_KVS_Fence(void) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_KVS_Get - returns the value associated with key in the key-value
  space associated with the job ID jobid

  Input Parameters:
  + jobid - the job id identifying the key-value space in which to look
  for key.  If jobid is NULL, look in the key-value space of this job.
  . src_pmi_id - the pmi id of the process which put this keypair.  This
  is just a hint to the server.  PMI2_ID_NULL should be passed if no
  hint is provided.
  . key - key
  - maxvalue - size of the buffer provided in value

  Output Parameters:
  + value - value associated with key
  - vallen - length of the returned value, or, if the length is longer
  than maxvalue, the negative of the required length is returned

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  @*/
int PMI2_KVS_Get(const char *jobid, int src_pmi_id, const char key[], char value [], int maxvalue, int *vallen) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Info_GetNodeAttr - returns the value of the attribute associated
  with this node

  Input Parameters:
  + name - name of the node attribute
  . valuelen - size of the buffer provided in value
  - waitfor - if non-zero, the function will not return until the
  attribute is available

  Output Parameters:
  + value - value of the attribute
  - found - non-zero indicates that the attribute was found

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  This provides a way, when combined with PMI2_Info_PutNodeAttr, for
  processes on the same node to share information without requiring a
  more general barrier across the entire job.

  If waitfor is non-zero, the function will never return with found
  set to zero.

  Predefined attributes:
  + memPoolType - If the process manager allocated a shared memory
  pool for the MPI processes in this job and on this node, return
  the type of that pool. Types include sysv, anonmmap and ntshm.
  . memSYSVid - Return the SYSV memory segment id if the memory pool
  type is sysv. Returned as a string.
  . memAnonMMAPfd - Return the FD of the anonymous mmap segment. The
  FD is returned as a string.
  - memNTName - Return the name of the Windows NT shared memory
  segment, file mapping object backed by system paging
  file.  Returned as a string.

  @*/
int PMI2_Info_GetNodeAttr(const char name[], char value[], int valuelen, int *found, int waitfor) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Info_GetNodeAttrIntArray - returns the value of the attribute associated
  with this node.  The value must be an array of integers.

  Input Parameters:
  + name - name of the node attribute
  - arraylen - number of elements in array

  Output Parameters:
  + array - value of attribute
  . outlen - number of elements returned
  - found - non-zero if attribute was found

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  Notice that, unlike PMI2_Info_GetNodeAttr, this function does not
  have a waitfor parameter, and will return immediately with found=0
  if the attribute was not found.

  Predefined array attribute names:
  + localRanksCount - Return the number of local ranks that will be
  returned by the key localRanks.
  . localRanks - Return the ranks in MPI_COMM_WORLD of the processes
  that are running on this node.
  - cartCoords - Return the Cartesian coordinates of this process in
  the underlying network topology. The coordinates are indexed from
  zero. Value only if the Job attribute for physTopology includes
  cartesian.

  @*/
int PMI2_Info_GetNodeAttrIntArray(const char name[], int array[], int arraylen, int *outlen, int *found) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Info_PutNodeAttr - stores the value of the named attribute
  associated with this node

  Input Parameters:
  + name - name of the node attribute
  - value - the value of the attribute

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Notes:
  For example, it might be used to share segment ids with other
  processes on the same SMP node.

  @*/
int PMI2_Info_PutNodeAttr(const char name[], const char value[]) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Info_GetJobAttr - returns the value of the attribute associated
  with this job

  Input Parameters:
  + name - name of the job attribute
  - valuelen - size of the buffer provided in value

  Output Parameters:
  + value - value of the attribute
  - found - non-zero indicates that the attribute was found

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  @*/
int PMI2_Info_GetJobAttr(const char name[], char value[], int valuelen, int *found) {
	pmi2_info_getjobattr_args_t *out;
	printf("%s: enterted looking up name %s valuelen %d\n", __func__, name, valuelen);
	fd = open("/dev/pmi", O_RDWR);
	out = malloc(sizeof(int) + sizeof(int) + valuelen);
	out->valuelen = valuelen;
// TODO(npe) errorpath
	ioctl(fd, PMI2_IOC_INFO_GETJOBATTR, out);
	*found = out->found;
	memcpy(value, out->value, valuelen);
	printf("%s: exited value %s valuelen %d *found %d\n", __func__, value, valuelen, *found);
	return PMI_SUCCESS;

	return 0;
}

/*@
  PMI2_Info_GetJobAttrIntArray - returns the value of the attribute associated
  with this job.  The value must be an array of integers.

  Input Parameters:
  + name - name of the job attribute
  - arraylen - number of elements in array

  Output Parameters:
  + array - value of attribute
  . outlen - number of elements returned
  - found - non-zero if attribute was found

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  Predefined array attribute names:

  + universeSize - The size of the "universe" (defined for the MPI
  attribute MPI_UNIVERSE_SIZE

  . hasNameServ - The value hasNameServ is true if the PMI2 environment
  supports the name service operations (publish, lookup, and
  unpublish).

  . physTopology - Return the topology of the underlying network. The
  valid topology types include cartesian, hierarchical, complete,
  kautz, hypercube; additional types may be added as necessary. If
  the type is hierarchical, then additional attributes may be
  queried to determine the details of the topology. For example, a
  typical cluster has a hierarchical physical topology, consisting
  of two levels of complete networks - the switched Ethernet or
  Infiniband and the SMP nodes. Other systems, such as IBM BlueGene,
  have one level that is cartesian (and in virtual node mode, have a
  single-level physical topology).

  . physTopologyLevels - Return a string describing the topology type
  for each level of the underlying network. Only valid if the
  physTopology is hierarchical. The value is a comma-separated list
  of physical topology types (except for hierarchical). The levels
  are ordered starting at the top, with the network closest to the
  processes last. The lower level networks may connect only a subset
  of processes. For example, for a cartesian mesh of SMPs, the value
  is cartesian,complete. All processes are connected by the
  cartesian part of this, but for each complete network, only the
  processes on the same node are connected.

  . cartDims - Return a string of comma-separated values describing
  the dimensions of the Cartesian topology. This must be consistent
  with the value of cartCoords that may be returned by
  PMI2_Info_GetNodeAttrIntArray.

  These job attributes are just a start, but they provide both an
  example of the sort of external data that is available through the
  PMI interface and how extensions can be added within the same API
  and wire protocol. For example, adding more complex network
  topologies requires only adding new keys, not new routines.

  . isHeterogeneous - The value isHeterogeneous is true if the
  processes belonging to the job are running on nodes with different
  underlying data models.

  @*/
int PMI2_Info_GetJobAttrIntArray(const char name[], int array[], int arraylen, int *outlen, int *found) {
	printf("%s: entered\n", __func__);
	return 0;
}
/*
  Input parameters:
  + service_name - string representing the service being published
  . info_ptr -
  - port - string representing the port on which to contact the service

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  @*/
int PMI2_Nameserv_publish(const char service_name[], const struct MPID_Info *info_ptr, const char port[]) {
	printf("%s: entered\n", __func__);
	return 0;
}

/*@
  PMI2_Nameserv_lookup - lookup a service by name

  Input parameters:
  + service_name - string representing the service being published
  . info_ptr -
  - portLen - size of buffer provided in port

  Output parameters:
  . port - string representing the port on which to contact the service

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  @*/
int PMI2_Nameserv_lookup(const char service_name[], const struct MPID_Info *info_ptr,
			 char port[], int portLen) {
	printf("%s: entered\n", __func__);
	return 0;
}
/*@
  PMI2_Nameserv_unpublish - unpublish a name

  Input parameters:
  + service_name - string representing the service being unpublished
  - info_ptr -

  Return values:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.

  @*/
int PMI2_Nameserv_unpublish(const char service_name[],
			    const struct MPID_Info *info_ptr)
{
	printf("%s: entered\n", __func__);
	return 0;
}

int PMI_Get_version_info(int *major,int *minor,int *revision) {
	printf("%s: entered\n", __func__);
	return 0;
}

int PMI_Allgather(void *in, void *out, int len) {
	printf("%s: entered\n", __func__);
	return 0;
}


int PMI_Allgatherv(void *in, int len, void *out, int *all_lens) {
	printf("%s: entered\n", __func__);
	return 0;
}

