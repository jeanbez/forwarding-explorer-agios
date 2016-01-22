/*
 * (C) 2001 Clemson University and The University of Chicago
 *
 * Changes by Acxiom Corporation to allow concurrency on operations that are
 * either read-only or do not modify specific objects
 * Copyright © Acxiom Corporation, 2005.
 *
 * See COPYING in top-level directory.
 */

/** \file
 *  \ingroup reqsched
 *
 *  An implementation of the server side request scheduler API.  
 *
 *  \note this is a prototype.  It simply hashes on the handle
 *  value in the request and builds a linked list for each handle.
 *  Only the request at the head of each list is allowed to proceed.
 */

/* LONG TERM
 * TODO: come up with a more sophisticated scheduling and
 *  consistency model
 */

#include <errno.h>
#include <stdlib.h>
#ifndef WIN32
#include <sys/time.h>
#endif
#include <assert.h>

#include "request-scheduler.h"
#include "quickhash.h"
#include "pvfs2-types.h"
#include "pvfs2-req-proto.h"
#include "pvfs2-debug.h"
#include "gossip.h"
#include "id-generator.h"
#include "pvfs2-internal.h"
#include "pvfs2-dist-simple-stripe.h"

/* we need the server header because it defines the operations that
 * we use to determine whether to schedule or queue.  
 *
 * TODO: To make the request scheduler more generic we 
 * should probably have callbacks that get defined in 
 * the server code that determine whether to queue or 
 * schedule, print the operation name, etc.
 */
#include "src/server/pvfs2-server.h"

#define ORANGEFS_AGIOS
#ifdef ORANGEFS_AGIOS
#include <agios.h>

#define AGIOS_CONFIGFILE "/tmp/agios.conf"

static PVFS_size last_stripe_size = 65536; //it should not be hardcoded. However, if the information does not come with the requests, what should we do? 
static struct client agios_clnt;

static int agios_is_it_a_server=1;

static int requests_sent_to_agios=0;
static int requests_received_from_agios=0;

#endif

/** request states */
enum req_sched_states
{
    /** request is queued up, cannot be processed yet */
    REQ_QUEUED,
    /** request is being processed */
    REQ_SCHEDULED,
    /** request could be processed, but caller has not asked for it
     * yet 
     */
    REQ_READY_TO_SCHEDULE,
    /** for timer events */
    REQ_TIMING,
};

/** linked lists to be stored at each hash table element */
struct req_sched_list
{
    struct qlist_head hash_link;
    struct qlist_head req_list;
    PVFS_handle handle;
};

/** linked list elements; one for each request in the scheduler */
struct req_sched_element
{
    enum PVFS_server_op op;
    struct qlist_head list_link;	/* ties it to a queue */
    struct qlist_head ready_link;	/* ties to ready queue */
    void *user_ptr;		/* user pointer */
    req_sched_id id;		/* unique identifier */
    struct req_sched_list *list_head;	/* points to head of queue */
    enum req_sched_states state;	/* state of this element */
    PVFS_handle handle;
    struct timeval tv;			/* used for timer events */
    /* indicates type of access needed by this op */
    enum PINT_server_req_access_type access_type;
    int mode_change; /* specifies that the element is a mode change */
    enum PVFS_server_mode mode; /* the mode to change to */
    long long offset;
    long long real_offset;
    long len;
    int type;
};


/* hash table */
static struct qhash_table *req_sched_table;

/* queue of requests that are ready for service (in case
 * test_world is called 
 */
static QLIST_HEAD(
    ready_queue);
static int ready_queue_len=0;
static pthread_mutex_t ready_queue_mutex=PTHREAD_MUTEX_INITIALIZER;

/* queue of timed operations */
static QLIST_HEAD(
    timer_queue);

/* queue of mode requests */
static QLIST_HEAD(
    mode_queue);

static int hash_handle(
    void *handle,
    int table_size);
static int hash_handle_compare(
    void *key,
    struct qlist_head *link);

/* count of how many items are known to the scheduler */
static int sched_count = 0;
/* mode of the scheduler */
static enum PVFS_server_mode current_mode = PVFS_SERVER_NORMAL_MODE;


/** returns current mode of server
 */
enum PVFS_server_mode PINT_req_sched_get_mode(void)
{
    return(current_mode);
}

void ready_queue_include_request(struct qlist_head *ready_link)
{
	pthread_mutex_lock(&ready_queue_mutex);
	qlist_add_tail(ready_link, &ready_queue);	
	ready_queue_len++;
//	fprintf(stderr, " agora temos %d reqs na ready_queue\n", ready_queue_len);
	pthread_mutex_unlock(&ready_queue_mutex);
}
void ready_queue_remove_request(struct qlist_head *ready_link)
{
	pthread_mutex_lock(&ready_queue_mutex);
	qlist_del(ready_link);
	ready_queue_len--;
//	fprintf(stderr, " agora nos temos %d reqs na ready_queue\n", ready_queue_len);
	pthread_mutex_unlock(&ready_queue_mutex);
}
short int is_ready_queue_empty(void)
{
	short int ret=0;
	pthread_mutex_lock(&ready_queue_mutex);
	if(qlist_empty(&ready_queue))
		ret = 1;
	pthread_mutex_unlock(&ready_queue_mutex);
	return ret;
}
struct req_sched_element *ready_queue_remove_next(void)
{
	struct req_sched_element *tmp_element=NULL;

	pthread_mutex_lock(&ready_queue_mutex);
	if(!qlist_empty(&ready_queue))
	{
		tmp_element = qlist_entry((ready_queue.next), struct req_sched_element, ready_link);
		qlist_del(&(tmp_element->ready_link));
		ready_queue_len--;
	//	fprintf(stderr, " agora nos temos %d reqs na ready_queue\n", ready_queue_len);
	}
	pthread_mutex_unlock(&ready_queue_mutex);

	return tmp_element;
}

/* setup and teardown */

#ifdef ORANGEFS_AGIOS
/*callback function called by the scheduler when one request is selected to process*/
void PINT_req_sched_agios_process(int64_t req_id)
{
	struct req_sched_element *element;

	requests_received_from_agios++;
//	fprintf(stderr, "PINT_req_sched_agios_process - request no %d to come back from AGIOS\n", requests_received_from_agios);

 	element = id_gen_fast_lookup(req_id); 

	if(element)
	{
		assert(element->state == REQ_QUEUED);
		element->state = REQ_READY_TO_SCHEDULE;
	//	fprintf(stderr, "ORANGEFS - requisicao offset %lld (real %lld) tamanho %ld passando pra READY", element->offset, element->real_offset, element->len);
		ready_queue_include_request(&(element->ready_link));
	}	
	else
	{
	//	fprintf(stderr, "ORANGEFS PANIC! Could not find request that came back from AGIOS!\n");
		exit(-1);
	}
}
void PINT_req_sched_agios_process_multiple(int64_t *reqs, int reqnb)
{
	struct req_sched_element *element;
	int i;

	requests_received_from_agios+=reqnb;

	for(i=0; i< reqnb; i++)
	{
 		element = id_gen_fast_lookup(reqs[i]); 

		if(element)
		{
			assert(element->state == REQ_QUEUED);
			element->state = REQ_READY_TO_SCHEDULE;
			ready_queue_include_request(&(element->ready_link));
		}	
		else
		{
			fprintf(stderr, "ORANGEFS PANIC! Could not find request that came back from AGIOS!\n");
			exit(-1);
		}
	}
}
#endif

/** Initializes the request scheduler.  Must be called before any other
 *  request scheduler routines.
 *
 *  \return 0 on success, -errno on failure
 */
int PINT_req_sched_initialize(int is_it_a_server)
{
    /* build hash table */
    req_sched_table = qhash_init(hash_handle_compare, hash_handle, 1021);
    if (!req_sched_table)
    {
	return (-ENOMEM);
    }

#ifdef ORANGEFS_AGIOS
	agios_is_it_a_server=is_it_a_server;	
	if(is_it_a_server)
	{
	    /*initialize agios*/
		agios_clnt.process_request = PINT_req_sched_agios_process;
		agios_clnt.process_requests = PINT_req_sched_agios_process_multiple;
		if(agios_init(&agios_clnt, AGIOS_CONFIGFILE) != 0)
		{	
			fprintf(stderr, "problem initializing agios!\n");
			return (-ENOMEM);
		}
	}
#endif

    return (0);
}

void PINT_req_sched_writestats(void)
{
#ifdef ORANGEFS_AGIOS
	agios_print_stats_file("/tmp/agios_statsfile.txt");	
	agios_reset_stats();
#endif
}


/** Free resources held by the timer queue
 */
int PINT_timer_queue_finalize(void)
{
   struct qlist_head *scratch=NULL;
   struct qlist_head *iterator=NULL;
   struct req_sched_element *element=NULL;

#ifdef ORANGEFS_AGIOS
	if(agios_is_it_a_server)
		//stop agios
		agios_exit();
#endif

   qlist_for_each_safe(iterator,scratch,&timer_queue)
   {
       element = qlist_entry(iterator,struct req_sched_element,list_link);
       qlist_del(&(element->list_link));
       if (element && element->user_ptr)
          free(element->user_ptr);
       if (element)
          free(element);
       element=NULL;
   }

  return(0);
}



/** Tears down the request scheduler and its data structures 
 *
 *  \return 0 on success, -errno on failure
 */
int PINT_req_sched_finalize(
    void)
{
    int i;
    struct req_sched_list *tmp_list;
    struct qlist_head *scratch;
    struct qlist_head *iterator;
    struct qlist_head *scratch2;
    struct qlist_head *iterator2;
    struct req_sched_element *tmp_element;

    /* iterate through the hash table */
    for (i = 0; i < req_sched_table->table_size; i++)
    {
	/* remove any queues from the table */
	qlist_for_each_safe(iterator, scratch, &(req_sched_table->array[i]))
	{
	    tmp_list = qlist_entry(iterator, struct req_sched_list,
				   hash_link);
	    /* remove any elements from each queue */
	    qlist_for_each_safe(iterator2, scratch2, &(tmp_list->req_list))
	    {
		tmp_element = qlist_entry(iterator2, struct req_sched_element,
					  list_link);
		free(tmp_element);
		/* note: no need to delete from list; we are
		 * destroying it as we go 
		 */
	    }
	    free(tmp_list);
	    /* note: no need to delete from list; we are destroying
	     * it as we go
	     */
	}
    }

    sched_count = 0;

    /* tear down hash table */
    qhash_finalize(req_sched_table);
    return (0);
}

int PINT_req_sched_change_mode(enum PVFS_server_mode mode,
                               void *user_ptr,
                               req_sched_id *id)
{
    int ret = -1;
    int mode_change_ready = 0;
    struct req_sched_element *mode_element;

    /* create a structure to store in the request queues */
    mode_element = (struct req_sched_element *) malloc(
        sizeof(struct req_sched_element));
    if (!mode_element)
    {
        return (-errno);
    }
    memset(mode_element, 0, sizeof(*mode_element));

    mode_element->user_ptr = user_ptr;
    id_gen_fast_register(id, mode_element);
    mode_element->id = *id;
    mode_element->state = REQ_QUEUED;
    mode_element->mode_change = 1;
    mode_element->mode = mode;
	mode_element->offset=0;
	mode_element->real_offset=0;
	mode_element->len = 0;

    /* will this be the front of the queue */
    if(qlist_empty(&mode_queue))
        mode_change_ready = 1;

    qlist_add_tail(&(mode_element->list_link), &mode_queue);
    if(mode_change_ready)
    {
        if(mode == PVFS_SERVER_NORMAL_MODE)
        {
            /* let this go through regardless */
            ret = 1;
            mode_element->state = REQ_SCHEDULED;
            current_mode = mode;
        }
        else if(mode == PVFS_SERVER_ADMIN_MODE)
        {
            assert(sched_count > -1);
            /* for this to work, we must wait for pending ops to complete */
            if(sched_count == 0)
            {
                ret = 1;
                mode_element->state = REQ_SCHEDULED;
                current_mode = mode;
            }
            else
            {
                ret = 0;
                mode_element->state = REQ_QUEUED;
            }
        }
        else
        {
            /* TODO: be nicer about this */
            assert(0);
        }
        return(ret);
    }
    else
    {
        mode_element->state = REQ_QUEUED;
        return(0);
    }
}

static int PINT_req_sched_in_admin_mode(void)
{
    struct req_sched_element *mode_element = NULL;
    if(!qlist_empty(&mode_queue))
        mode_element = qlist_entry(mode_queue.next, struct req_sched_element,
                                   list_link);
    if(current_mode == PVFS_SERVER_ADMIN_MODE ||
       (mode_element && mode_element->mode == PVFS_SERVER_ADMIN_MODE))
    {
        return 1;
    }
    return 0;
}

static int PINT_req_sched_schedule_mode_change(void)
{
    struct req_sched_element *next_element;

    /* prepare to schedule mode change if we can */
    /* NOTE: only transitions to admin mode are ever queued */
    if(sched_count == 0 && !qlist_empty(&mode_queue))
    {
	next_element = qlist_entry(mode_queue.next, struct req_sched_element,
	    list_link);
	next_element->state = REQ_READY_TO_SCHEDULE;
	ready_queue_include_request(&(next_element->ready_link));
    }
    return 0;
}

static void PINT_req_sched_do_change_mode(
    struct req_sched_element *req_sched_element)
{
    if(req_sched_element->mode_change)
    {
        current_mode = req_sched_element->mode;
    }
}

/* scheduler submission */

/** Posts an incoming request to the scheduler
 *
 *  \return 1 if request should proceed immediately, 0 if the
 *  request will be scheduled later, and -errno on failure
 */
int PINT_req_sched_post(enum PVFS_server_op op,
                        PVFS_fs_id fs_id,
                        PVFS_handle handle,
                        enum PINT_server_req_access_type access_type,
                        enum PINT_server_sched_policy sched_policy,
			void *in_user_ptr,
			req_sched_id * out_id,
			void *s_op)
{
    struct qlist_head *hash_link;
    int ret = -1;
    struct req_sched_element *tmp_element;
    struct req_sched_element *tmp_element2;
    struct req_sched_list *tmp_list;
    struct req_sched_element *next_element;
    struct req_sched_element *last_element;
    struct qlist_head *iterator;
    int tmp_flag;
#ifdef ORANGEFS_AGIOS
	short int first_file_req=0;
#endif

	/*test_out = fopen(TEST_REQATTS_FILENAME, "a");
	if(test_out)
	{
		fprintf(test_out, "req chegando na PINT_req_sched_post. op = %d. in_user_ptr = NULL? ", op);
		if(in_user_ptr)
			fprintf(test_out, "nop.\n");
		else
			fprintf(test_out, "yep.\n");
		fclose(test_out);
	}
	else
		fprintf(stderr, "ops!");*/
//	printf("req chegando na PINT_req_sched_post. op = %d\n", op);

    if(sched_policy == PINT_SERVER_REQ_BYPASS)
    {
        if(access_type == PINT_SERVER_REQ_MODIFY && !PVFS_SERV_IS_MGMT_OP(op))
        {
            /* if this requests modifies the file system, we have to check
             * to see if we are in admin mode or about to enter admin mode
             */
            if(PINT_req_sched_in_admin_mode())
            {
                return (-PVFS_EAGAIN);
            }
        }

#ifdef ORANGEFS_AGIOS
	if(!agios_is_it_a_server)
	{
#endif
	        /* no special mode; allow request to proceed immediately */
        	*out_id = 0;
	        return(1);
#ifdef ORANGEFS_AGIOS
	}
#endif
    }
#ifdef ORANGEFS_AGIOS
	if((op == PVFS_SERV_MGMT_WRITESTATS) && (agios_is_it_a_server))
	{
		PINT_req_sched_writestats();
		*out_id = 0;
		return(1);
	}
#endif

    /* NOTE: handle == 0 is a special case, the request isn't
     * operating on a particular handle, but we will queue anyway
     * on handle == 0 for the moment...
     */

    /* create a structure to store in the request queues */
    tmp_element = (struct req_sched_element *) malloc(sizeof(struct
							     req_sched_element));
    if (!tmp_element)
    {
	return (-errno);
    }
    memset(tmp_element, 0, sizeof(*tmp_element));

    tmp_element->op = op;
    tmp_element->user_ptr = in_user_ptr;
    id_gen_fast_register(out_id, tmp_element);
    tmp_element->id = *out_id;
    tmp_element->state = REQ_QUEUED;
    tmp_element->handle = handle;
    tmp_element->list_head = NULL;
    tmp_element->access_type = access_type;
    tmp_element->mode_change = 0;
	tmp_element->offset=0;
	tmp_element->real_offset=0;
	tmp_element->len = 0;

    if(access_type == PINT_SERVER_REQ_MODIFY && !PVFS_SERV_IS_MGMT_OP(op))
    {
        if(PINT_req_sched_in_admin_mode())
        {
            free(tmp_element);
            return(-PVFS_EAGAIN);
        }
    }

    /* see if we have a request queue up for this handle */
    hash_link = qhash_search(req_sched_table, &(handle));
    if (hash_link)
    {
	/* we already have a queue for this handle */
	tmp_list = qlist_entry(hash_link, struct req_sched_list,
			       hash_link);
    }
    else
    {
#ifdef ORANGEFS_AGIOS
	first_file_req=1;
#endif
	/* no queue yet for this handle */
	/* create one and add it in */
	tmp_list = (struct req_sched_list *)malloc(
            sizeof(struct req_sched_list));
	if (!tmp_list)
	{
	    free(tmp_element);
	    return (-ENOMEM);
	}

	tmp_list->handle = handle;
	INIT_QLIST_HEAD(&(tmp_list->req_list));

	qhash_add(req_sched_table, &(handle), &(tmp_list->hash_link));

    }

    /* at either rate, we now have a pointer to the list head */

#ifdef ORANGEFS_AGIOS
	if((agios_is_it_a_server) && ((op == PVFS_SERV_IO) || (op == PVFS_SERV_SMALL_IO)))
	{
		tmp_element->state = REQ_QUEUED;
		ret = 0;
	}
	else
	{
#endif
    /* return 1 if the list is empty before we add this entry */
    ret = qlist_empty(&(tmp_list->req_list));
    if (ret == 1)
    {
	tmp_element->state = REQ_SCHEDULED;
    }
    else
    {
        /* check queue to see if we can apply any optimizations */
        /* first check: are all current queued operations already scheduled
         * (ie, is the first and last scheduled?).  We can never bypass a
         * queued operation 
         */
	next_element = qlist_entry((tmp_list->req_list.next),
				   struct req_sched_element,
				   list_link);
	last_element = qlist_entry((tmp_list->req_list.prev),
				   struct req_sched_element,
				   list_link);
	if (op == PVFS_SERV_IO &&
	    next_element->state == REQ_SCHEDULED &&
	    last_element->state == REQ_SCHEDULED)
	{
            /* possible I/O optimization: see if all scheduled ops for this
             * handle are for I/O.  If so, we can allow another concurrent
             * I/O request to proceed 
             */
            tmp_flag = 0;
            qlist_for_each(iterator, &tmp_list->req_list)
            {
                tmp_element2 = qlist_entry(iterator, struct req_sched_element,
                    list_link);
                if(tmp_element2->op != PVFS_SERV_IO)
                {
                    tmp_flag = 1;
                    break;
                }
            }

            if(!tmp_flag)
            {
                tmp_element->state = REQ_SCHEDULED;
                ret = 1;
                gossip_debug(GOSSIP_REQ_SCHED_DEBUG, "REQ SCHED allowing "
                             "concurrent I/O, handle: %llu\n", llu(handle));
            }
            else
            {
                tmp_element->state = REQ_QUEUED;
                ret = 0;
            }
	}
	else if (access_type == PINT_SERVER_REQ_READONLY &&
	    next_element->state == REQ_SCHEDULED &&
	    last_element->state == REQ_SCHEDULED)
        {
            /* possible read only optimization: see if all scheduled ops 
             * for this handle are read only.  If so, we can allow another 
             * concurrent read only request to proceed 
             */
            tmp_flag = 0;
            qlist_for_each(iterator, &tmp_list->req_list)
            {
                tmp_element2 = qlist_entry(iterator, struct req_sched_element,
                    list_link);
                if(tmp_element2->access_type == PINT_SERVER_REQ_MODIFY)
                {
                    tmp_flag = 1;
                    break;
                }
            }

            if(!tmp_flag)
            {
                tmp_element->state = REQ_SCHEDULED;
                ret = 1;
                gossip_debug(GOSSIP_REQ_SCHED_DEBUG, "REQ SCHED allowing "
                             "concurrent read only, handle: %llu\n", llu(handle));
            }
            else
            {
                tmp_element->state = REQ_QUEUED;
                ret = 0;
            }
        }
        else if((op == PVFS_SERV_CRDIRENT || op == PVFS_SERV_RMDIRENT) &&
                next_element->state == REQ_SCHEDULED &&
                last_element->state == REQ_SCHEDULED)
        {
            /* possible dirent optimization: see if all scheduled ops for this
             * handle are for crdirent or rmdirent.  
             * If so, we can allow another concurrent
             * dirent request to proceed.
             */
            tmp_element->state = REQ_SCHEDULED;
            tmp_element->access_type = PINT_SERVER_REQ_READONLY;
            gossip_debug(GOSSIP_REQ_SCHED_DEBUG, "REQ SCHED allowing "
                         "concurrent dirent op, handle: %llu\n", 
                         llu(handle));
            ret = 1;
        }
	else
	{
	    tmp_element->state = REQ_QUEUED;
	    ret = 0;
	}
    }
#ifdef ORANGEFS_AGIOS
	}
#endif
    /* add this element to the list */
    tmp_element->list_head = tmp_list;
    qlist_add_tail(&(tmp_element->list_link), &(tmp_list->req_list));

    gossip_debug(GOSSIP_REQ_SCHED_DEBUG,
		 "REQ SCHED POSTING, handle: %llu, queue_element: %p\n",
                 llu(handle), tmp_element);

    if (ret == 1)
    {
	gossip_debug(GOSSIP_REQ_SCHED_DEBUG, "REQ SCHED SCHEDULING, "
                     "handle: %llu, queue_element: %p\n",
		     llu(handle), tmp_element);
    }
    sched_count++;

#ifdef ORANGEFS_AGIOS
	if((agios_is_it_a_server) && ((op == PVFS_SERV_IO) || (op == PVFS_SERV_SMALL_IO)) && (s_op != NULL))
	{
		PVFS_offset offset;
		PVFS_size size, aux_size, stripe_size=-1;
		uint32_t server_nb, this_server, aux_server;
		char agios_fn[25];
	
		/*get filename*/	
		sprintf(agios_fn, "%llu", llu(handle));

		/*get offset and size*/
		if(op == PVFS_SERV_IO)
		{
			offset = ((PINT_server_op *) s_op)->req->u.io.file_req_offset ;
			size = ((PINT_server_op *) s_op)->req->u.io.aggregate_size;
			server_nb = ((PINT_server_op *) s_op)->req->u.io.server_ct;
			this_server= ((PINT_server_op *) s_op)->req->u.io.server_nr;
			if(((PINT_server_op *) s_op)->req->u.io.io_dist)
			{
				if(((PINT_server_op *) s_op)->req->u.io.io_dist->params)
					stripe_size = ( (PVFS_simple_stripe_params*) (((PINT_server_op *) s_op)->req->u.io.io_dist->params))->strip_size;
			}
		}
		else
		{
			offset = ((PINT_server_op *) s_op)->req->u.small_io.file_req_offset;
			size = ((PINT_server_op *) s_op)->req->u.small_io.aggregate_size;
			server_nb = ((PINT_server_op *) s_op)->req->u.small_io.server_ct;
			this_server = ((PINT_server_op *) s_op)->req->u.small_io.server_nr;
			if(((PINT_server_op *) s_op)->req->u.small_io.dist)
                        {
                                if(((PINT_server_op *) s_op)->req->u.small_io.dist->params)
                                        stripe_size = ( (PVFS_simple_stripe_params*) (((PINT_server_op *) s_op)->req->u.small_io.dist->params))->strip_size;
                        }

		}
		if(stripe_size == -1) //the information did not come with the request! what should we do?? TODO
			stripe_size = last_stripe_size;
		else
			last_stripe_size = stripe_size;

		//if this is the first request to this file, update AGIOS' stripe size
		if(first_file_req)
			agios_set_stripe_size(agios_fn, stripe_size);

		//get the offset in the local file (offset from the request is about the global view of the file)
		tmp_element->offset = offset;
		if(server_nb > 0)
			tmp_element->real_offset = (offset % stripe_size) + (offset / (stripe_size * server_nb))*stripe_size;
		else
		{
			tmp_element->real_offset = offset;
		}
		//get the actual size (aggregated_size which comes with the request corresponds to the whole amount asked for the client, including to other servers)
		if(size <= stripe_size) //if the request was smaller than the stripe size, it has arrived with the right size here
			tmp_element->len = size;
		else //if the request was larger, than the size includes the portions requested from the other servers
		{
			tmp_element->len=0;
			aux_server = 0;
			aux_size=0;
			do {
				if((size - aux_size) >= stripe_size)
				{
					if(aux_server == this_server)
						tmp_element->len += stripe_size;
				}
				else
				{
					if(aux_server == this_server)
						tmp_element->len += size - aux_size;
				}
				aux_size += stripe_size;
				aux_server++;
				if(aux_server >= server_nb)
					aux_server = 0;
			} while (aux_size < size);
		}

		/*get operation type*/
		if(((PINT_server_op *) s_op)->req->u.io.io_type == PVFS_IO_READ)
			tmp_element->type = RT_READ;
		else
			tmp_element->type = RT_WRITE;

		/*give the request to agios*/
		requests_sent_to_agios+=1;
//		fprintf(stderr, "this is the request no %d to go to AGIOS\n", requests_sent_to_agios);
		agios_add_request(agios_fn, tmp_element->type, tmp_element->real_offset, tmp_element->len, tmp_element->id, &agios_clnt);

	}
#endif

    return (ret);
}


/** posts a timer - will complete like a normal request at approximately 
 *  the interval specified
 *
 *  \return 1 on immediate completion, 0 if caller should test later,
 *  -errno on failure
 */
int PINT_req_sched_post_timer(
    int msecs,
    void *in_user_ptr,
    req_sched_id * out_id)
{
    struct req_sched_element *tmp_element;
    struct req_sched_element *next_element;
    struct qlist_head* scratch;
    struct qlist_head* iterator;
    int found = 0;

    if(msecs < 1)
	return(1);

    /* create a structure to store in the request queues */
    tmp_element = (struct req_sched_element *)malloc(
        sizeof(struct req_sched_element));
    if (!tmp_element)
    {
	return (-errno);
    }
    memset(tmp_element, 0, sizeof(*tmp_element));

    tmp_element->user_ptr = in_user_ptr;
    id_gen_fast_register(out_id, tmp_element);
    tmp_element->id = *out_id;
    tmp_element->state = REQ_TIMING;
    tmp_element->handle = PVFS_HANDLE_NULL;
    gettimeofday(&tmp_element->tv, NULL);
    tmp_element->list_head = NULL;
    tmp_element->mode_change = 0;
	tmp_element->offset=0;
	tmp_element->real_offset=0;
	tmp_element->len = 0;

    /* set time to future, based on msecs arg */
    tmp_element->tv.tv_sec += msecs/1000;
    tmp_element->tv.tv_usec += (msecs%1000)*1000;
    if(tmp_element->tv.tv_usec > 1000000)
    {
	tmp_element->tv.tv_sec += tmp_element->tv.tv_usec / 1000000;
	tmp_element->tv.tv_usec = tmp_element->tv.tv_usec % 1000000;
    }

    /* put in timer queue, in order */
    qlist_for_each_safe(iterator, scratch, &timer_queue)
    {
	/* look for first entry that comes after our current one */
	next_element = qlist_entry(iterator, struct req_sched_element,
	    list_link);
	if((next_element->tv.tv_sec > tmp_element->tv.tv_sec)
	    || (next_element->tv.tv_sec == tmp_element->tv.tv_sec 
		&& next_element->tv.tv_usec > tmp_element->tv.tv_usec))
	{
	    found = 1;
	    break;
	}
    }

    /* either stick it in the middle somewhere or in the back */
    if(found)
    {
	__qlist_add(&tmp_element->list_link, iterator->prev, iterator);
    }
    else
    {
	qlist_add_tail(&tmp_element->list_link, &timer_queue);
    }

#if 0
    gossip_debug(GOSSIP_REQ_SCHED_DEBUG,
		 "REQ SCHED POSTING, queue_element: %p\n",
		 tmp_element);
#endif
    return(0);
}


/** Removes a request from the scheduler before it has even been
 *  scheduled
 *
 *  \return 0 on success, -errno on failure 
 */
/*this could be a problem with AGIOS (remove the req_sched_element structure, but it is still on the agios queue), but i could not find any calls to this function, so i'll just ignore it*/
int PINT_req_sched_unpost(
    req_sched_id in_id,
    void **returned_user_ptr)
{
    struct req_sched_element *tmp_element = NULL;
    struct req_sched_element *next_element = NULL;
    int next_ready_flag = 0;

//fprintf(stderr, "PINT_req_sched_unpost start");
    /* NOTE: we set the next_ready_flag to 1 if the next element in
     * the queue should be put in the ready list 
     */

    /* retrieve the element directly from the id */
    tmp_element = id_gen_fast_lookup(in_id);

    /* make sure it isn't already scheduled */
    if (tmp_element->state == REQ_SCHEDULED)
    {
	return (-EALREADY);
    }

    if (tmp_element->state == REQ_READY_TO_SCHEDULE)
    {
	ready_queue_remove_request(&(tmp_element->ready_link));
	next_ready_flag = 1;
	/* fall through on purpose */
    }

    if (returned_user_ptr)
    {
	returned_user_ptr[0] = tmp_element->user_ptr;
    }

    qlist_del(&(tmp_element->list_link));

    /* special operations, like mode changes, may not be associated with a list */
    if(tmp_element->list_head)
    {
	/* see if there is another request queued behind this one */
	if (qlist_empty(&(tmp_element->list_head->req_list)))
	{
	    /* queue now empty, remove from hash table and destroy */
	    qlist_del(&(tmp_element->list_head->hash_link));
	    free(tmp_element->list_head);
	}
	else
	{
#ifdef ORANGEFS_AGIOS
	if(!agios_is_it_a_server)
	{
#endif
	    /* queue not empty, prepare next request in line for
	     * processing if necessary
	     */
	    if (next_ready_flag)
	    {
		next_element =
		    qlist_entry((tmp_element->list_head->req_list.next),
				struct req_sched_element,
				list_link);
		/* skip looking at the next request if it is already
		 * ready to go 
		 */
		if (next_element->state != REQ_READY_TO_SCHEDULE &&
		    next_element->state != REQ_SCHEDULED)
		{
		    next_element->state = REQ_READY_TO_SCHEDULE;
		    ready_queue_include_request(&(next_element->ready_link));
		    /* keep going as long as the operations are I/O requests;
		     * we let these all go concurrently
		     */
		    while (next_element && next_element->op == PVFS_SERV_IO
			   && next_element->list_link.next !=
			   &(tmp_element->list_head->req_list))
		    {
			next_element =
			    qlist_entry(next_element->list_link.next,
					struct req_sched_element,
					list_link);
			if (next_element
			    && next_element->op == PVFS_SERV_IO)
			{
			    gossip_debug(
                                GOSSIP_REQ_SCHED_DEBUG, "REQ SCHED "
                                "allowing concurrent I/O, handle: %llu\n",
                                llu(next_element->handle));
			    next_element->state = REQ_READY_TO_SCHEDULE;
		    	    ready_queue_include_request(&(next_element->ready_link));
			}
		    }
		}
	    }
#ifdef ORANGEFS_AGIOS
	} //if(!agios_is_it_a_server)
#endif
	}
	sched_count--;
    }

    /* destroy the unposted element */
    free(tmp_element);

    PINT_req_sched_schedule_mode_change();
    return (0);
}

/** releases a completed request from the scheduler, potentially
 *  allowing other requests to proceed 
 *
 *  \return 1 on immediate successful completion, 0 to test later,
 *  -errno on failure
 */
int PINT_req_sched_release(
    req_sched_id in_completed_id,
    void *in_user_ptr,
    req_sched_id * out_id)
{
    struct req_sched_element *tmp_element = NULL;
    struct req_sched_list *tmp_list = NULL;
    struct req_sched_element *next_element = NULL;
    //enum PVFS_server_op op;

//	fprintf(stderr, "PINT_req_sched_release start\n");

    /* NOTE: for now, this function always returns immediately- no
     * need to fill in the out_id
     */
    *out_id = 0;

    if(in_completed_id == 0)
    {
        /* the scheduler let this operation pass through; no infrastructure
         * to clean up
         */
        return(1);
    }

    /* retrieve the element directly from the id */
    tmp_element = id_gen_fast_lookup(in_completed_id);
//	op = tmp_element->op;
//	fprintf(stderr, "PINT_req_sched_release op = %d, id= %d\n", op, tmp_element->id);


	/*notify AGIOS about request processing so it can keep performance measurements (and manage the synchronous approach)*/
	if((agios_is_it_a_server) && ((tmp_element->op == PVFS_SERV_IO) || (tmp_element->op == PVFS_SERV_SMALL_IO)))
	{
		char agios_fn[25];
	
		/*get filename*/	
		sprintf(agios_fn, "%llu", llu(tmp_element->handle));

		/*release request*/
		agios_release_request(agios_fn, tmp_element->type, tmp_element->len, tmp_element->real_offset);
	}

    /* remove it from its handle queue */
    qlist_del(&(tmp_element->list_link));

    /* find the top of the queue */
    tmp_list = tmp_element->list_head;

    /* special operations, like mode changes, may not be associated w/ a list */
    if(tmp_list)
    {
	/* find out if there is another operation queued behind it or
	 * not 
	 */
	if (qlist_empty(&(tmp_list->req_list)))
	{
	    /* nothing else in this queue, remove it from the hash table
	     * and deallocate 
	     */
	    qlist_del(&(tmp_list->hash_link));
	    free(tmp_list);
	}
	else
	{

	    /* something is queued behind this request */
	    /* find the next request, change its state, and add it to
	     * the queue of requests that are ready to be scheduled
	     */
	    next_element = qlist_entry((tmp_list->req_list.next),
				       struct req_sched_element,
				       list_link);
	    
            /* skip it if the top queue item is already ready for
	     * scheduling 
	     */
	    if (next_element->state != REQ_READY_TO_SCHEDULE &&
		next_element->state != REQ_SCHEDULED)
	    {
#ifdef ORANGEFS_AGIOS
		if((!agios_is_it_a_server) || ((agios_is_it_a_server) && (next_element->op != PVFS_SERV_IO) && (next_element->op != PVFS_SERV_SMALL_IO)))
                  {
#endif
			next_element->state = REQ_READY_TO_SCHEDULE;
			ready_queue_include_request(&(next_element->ready_link));

                	if(next_element->op == PVFS_SERV_IO)
	                {
        	            /* keep going as long as the operations are I/O requests;
                	     * we let these all go concurrently
	                     */
        	            while (next_element &&
                	           (next_element->op == PVFS_SERV_IO) &&
                        	   (next_element->list_link.next != &(tmp_list->req_list)))
	                    {
        	                next_element =
                	            qlist_entry(next_element->list_link.next,
                        	                struct req_sched_element,
                                	        list_link);
	                        if (next_element &&
        	                    (next_element->op == PVFS_SERV_IO))
                	        {
                        	    gossip_debug(
                                	GOSSIP_REQ_SCHED_DEBUG,
	                                "REQ SCHED allowing concurrent I/O (release time), "
        	                        "handle: %llu\n", llu(next_element->handle));
                	            assert(next_element->state == REQ_QUEUED);
                        	    next_element->state = REQ_READY_TO_SCHEDULE;
				    ready_queue_include_request(&(next_element->ready_link));
        	                }
                	    }
	                }
#ifdef ORANGEFS_AGIOS
		}
#else
		else
#endif
                if(next_element->access_type == PINT_SERVER_REQ_READONLY)
	                {
        	            /* keep going as long as the operations are read only;
                	     * we let these all go concurrently
	                     */
        	            while (next_element &&
                	           (next_element->access_type == PINT_SERVER_REQ_READONLY) &&
                        	   (next_element->list_link.next != &(tmp_list->req_list)))
	                    {
        	                next_element =
                	            qlist_entry(next_element->list_link.next,
                        	                struct req_sched_element,
                                	        list_link);
	                        if (next_element &&
        	                    (next_element->access_type == PINT_SERVER_REQ_READONLY))
                	        {
#ifdef ORANGEFS_AGIOS
					if((!agios_is_it_a_server) || ((agios_is_it_a_server) && (next_element->op != PVFS_SERV_IO) && (next_element->op != PVFS_SERV_SMALL_IO)))
					{
#endif
        				gossip_debug(
		                                GOSSIP_REQ_SCHED_DEBUG,
                		                "REQ SCHED allowing concurrent read only (release time), "
                                		"handle: %llu\n", llu(next_element->handle));
	                                assert(next_element->state == REQ_QUEUED);
                	                next_element->state = REQ_READY_TO_SCHEDULE;
		       			ready_queue_include_request(&(next_element->ready_link));
#ifdef ORANGEFS_AGIOS
					}
#endif
                        	} //end if (next_element && (next_element->access_type == PINT_SERVER_REQ_READONLY))
	  		    } // end while 
    	               } //end else if(next_element->access_type == PINT_SERVER_REQ_READONLY)
	    } //end if (next_element->state != REQ_READY_TO_SCHEDULE && next_element->state != REQ_SCHEDULED)
	} //end else
	sched_count--;
    } //end if(tmp_list)

    gossip_debug(GOSSIP_REQ_SCHED_DEBUG,
		 "REQ SCHED RELEASING, handle: %llu, queue_element: %p\n",
		 llu(tmp_element->handle), tmp_element);

    /* destroy the released request element */
    free(tmp_element);

    PINT_req_sched_schedule_mode_change();
//	fprintf(stderr, "PINT_req_sched_release leaving\n");
    return (1);
}

/* testing for completion */

/** Tests for completion of a single scheduler operation
 *
 *  \return 0 on success, -errno on failure
 */
int PINT_req_sched_test(
    req_sched_id in_id,
    int *out_count_p,
    void **returned_user_ptr_p,
    req_sched_error_code * out_status)
{
    struct req_sched_element *tmp_element = NULL;
    struct timeval tv;

    *out_count_p = 0;

//	fprintf(stderr, "ORANGEFS PINT_req_sched_test start\n");

    /* retrieve the element directly from the id */
    tmp_element = id_gen_fast_lookup(in_id);

    /* sanity check the state */
    if (tmp_element->state == REQ_SCHEDULED)
    {
	/* it's already scheduled! */
	//fprintf(stderr, "ORANGEFS PINT_req_sched_test leaving\n");
	return (-EINVAL);
    }
    else if (tmp_element->state == REQ_QUEUED)
    {
	/* it still isn't ready to schedule */
	//fprintf(stderr, "ORANGEFS PINT_req_sched_test leaving\n");
	return (0);
    }
    else if (tmp_element->state == REQ_READY_TO_SCHEDULE)
    {
	//fprintf(stderr, "PINT_req_sched_test scheduling a request");
	/* let it roll */
	tmp_element->state = REQ_SCHEDULED;
	/* remove from ready queue */
	ready_queue_remove_request(&(tmp_element->ready_link));

	if (returned_user_ptr_p)
	{
	    returned_user_ptr_p[0] = tmp_element->user_ptr;
	}
	*out_count_p = 1;
	*out_status = 0;
	gossip_debug(GOSSIP_REQ_SCHED_DEBUG, "REQ SCHED SCHEDULING, "
                     "handle: %llu, queue_element: %p\n",
                     llu(tmp_element->handle), tmp_element);

        PINT_req_sched_do_change_mode(tmp_element);
//	fprintf(stderr, "ORANGEFS PINT_req_sched_test leaving\n");
        return (1);
    }
    else if (tmp_element->state == REQ_TIMING)
    {
	/* timer event, see if we have hit time value yet */
	gettimeofday(&tv, NULL);
	if ((tmp_element->tv.tv_sec < tv.tv_sec) ||
	    (tmp_element->tv.tv_sec == tv.tv_sec &&
             tmp_element->tv.tv_usec < tv.tv_usec))
	{
	    /* time to go */
	    qlist_del(&(tmp_element->list_link));
	    if (returned_user_ptr_p)
	    {
		returned_user_ptr_p[0] = tmp_element->user_ptr;
	    }
	    *out_count_p = 1;
	    *out_status = 0;
	    gossip_debug(GOSSIP_REQ_SCHED_DEBUG,
			 "REQ SCHED TIMER SCHEDULING, queue_element: %p\n",
			 tmp_element);
	    free(tmp_element);
//	fprintf(stderr, "ORANGEFS PINT_req_sched_test leaving\n");
	    return (1);
	}
	else
	{
//	fprintf(stderr, "ORANGEFS PINT_req_sched_test leaving\n");
	    return(0);
	}
    }
    else
    {
//	fprintf(stderr, "ORANGEFS PINT_req_sched_test leaving\n");
        /* should not hit this point */
	return (-EINVAL);
    }
}

/** Tests for completion of one or more of a set of scheduler operations.
 */
int PINT_req_sched_testsome(
    req_sched_id * in_id_array,
    int *inout_count_p,
    int *out_index_array,
    void **returned_user_ptr_array,
    req_sched_error_code * out_status_array)
{
    struct req_sched_element *tmp_element = NULL;
    int i;
    int incount = *inout_count_p;
    struct timeval tv;

    *inout_count_p = 0;
//	fprintf(stderr, "ORANGEFS PINT_req_sched_testsome start\n");

    /* if there are any pending timer events, go ahead and get the 
     * current time so that we are ready if we run across one
     */
    if(!qlist_empty(&timer_queue))
    {
	gettimeofday(&tv, NULL);
    }
//	fprintf(stderr, "ORANGEFS PINT_req_sched_testsome is going to schedule %d requests\n",incount);

    for (i = 0; i < incount; i++)
    {
	/* retrieve the element directly from the id */
	tmp_element = id_gen_fast_lookup(in_id_array[i]);

	/* sanity check the state */
	if (tmp_element->state == REQ_SCHEDULED)
	{
	    /* it's already scheduled! */
	    return (-EINVAL);
	}
	else if (tmp_element->state == REQ_QUEUED)
	{
	    /* it still isn't ready to schedule */
	    /* do nothing */
	}
	else if (tmp_element->state == REQ_READY_TO_SCHEDULE)
	{
	//	fprintf(stderr, "ORANGEFS PINT_req_sched_testsome scheduling request no %d \n", i);
	    /* let it roll */
	    tmp_element->state = REQ_SCHEDULED;
	    /* remove from ready queue, leave in hash table queue */
	    ready_queue_remove_request(&(tmp_element->ready_link));
	    if (returned_user_ptr_array)
	    {
		returned_user_ptr_array[*inout_count_p] = tmp_element->user_ptr;
	    }
	    out_index_array[*inout_count_p] = i;
	    out_status_array[*inout_count_p] = 0;
	    (*inout_count_p)++;
	    gossip_debug(GOSSIP_REQ_SCHED_DEBUG,
			 "REQ SCHED SCHEDULING, handle: %llu, "
                         "queue_element: %p\n",
			 llu(tmp_element->handle), tmp_element);
            PINT_req_sched_do_change_mode(tmp_element);
	}
	else if(tmp_element->state == REQ_TIMING)
	{
	    /* timer event, see if we have hit time value yet */
	    gettimeofday(&tv, NULL);
	    if((tmp_element->tv.tv_sec < tv.tv_sec) ||
		(tmp_element->tv.tv_sec == tv.tv_sec 
		    && tmp_element->tv.tv_usec < tv.tv_usec))
	    {
		/* time to go */
		qlist_del(&(tmp_element->list_link));
		if (returned_user_ptr_array)
		{
		    returned_user_ptr_array[*inout_count_p] = 
			tmp_element->user_ptr;
		}
		out_index_array[*inout_count_p] = i;
		out_status_array[*inout_count_p] = 0;
		(*inout_count_p)++;
		gossip_debug(GOSSIP_REQ_SCHED_DEBUG,
			     "REQ SCHED TIMER SCHEDULING, queue_element: %p\n",
			     tmp_element);
		free(tmp_element);
	    }
	}
	else
	{
	//	fprintf(stderr, "ORANGEFS PINT_req_sched_testsome leaving\n");
	    return (-EINVAL);
	}
    }
	//	fprintf(stderr, "ORANGEFS PINT_req_sched_testsome leaving\n");
    if (*inout_count_p > 0)
	return (1);
    else
	return (0);
}

/** Tests for completion of any scheduler request.
 */
int PINT_req_sched_testworld(
    int *inout_count_p,
    req_sched_id * out_id_array,
    void **returned_user_ptr_array,
    req_sched_error_code * out_status_array)
{
    int incount = *inout_count_p;
    struct req_sched_element *tmp_element;
    struct qlist_head* scratch;
    struct qlist_head* iterator;
    struct timeval tv;

    *inout_count_p = 0;
//	fprintf(stderr, "ORANGEFS PINT_req_sched_testworld start");
//	if(is_ready_queue_empty())
//		fprintf(stderr, "ready_queue is empty\n");
//	else
//		fprintf(stderr, "ready_queue is NOT empty\n");

    /* do timers first, if we have them */
    if(!qlist_empty(&timer_queue))
    {
	gettimeofday(&tv, NULL);
	qlist_for_each_safe(iterator, scratch, &timer_queue)
	{
	    tmp_element = qlist_entry(iterator, struct req_sched_element,
		list_link);
	    if((tmp_element->tv.tv_sec > tv.tv_sec) 
		|| (tmp_element->tv.tv_sec == tv.tv_sec && 
		    tmp_element->tv.tv_usec > tv.tv_usec))
	    {	
		break;
	    }
	    else
	    {
		qlist_del(&(tmp_element->list_link));
		out_id_array[*inout_count_p] = tmp_element->id;
		if (returned_user_ptr_array)
		{
		    returned_user_ptr_array[*inout_count_p] = tmp_element->user_ptr;
		}
		out_status_array[*inout_count_p] = 0;
		(*inout_count_p)++;
#if 0
		gossip_debug(GOSSIP_REQ_SCHED_DEBUG,
			     "REQ SCHED SCHEDULING, queue_element: %p\n",
			     tmp_element);
#endif
		free(tmp_element);
		if(*inout_count_p == incount)
		    break;
	    }
	}
    }

    while ((!is_ready_queue_empty()) && (*inout_count_p < incount))
    {
//	fprintf(stderr, "ORANGEFS PINT_req_sched_testworld scheduling a request...");
	tmp_element = ready_queue_remove_next();
	if(!tmp_element)
		continue;
//	fprintf(stderr, "...op = %d, id = %d\n", tmp_element->op, tmp_element->id);
	out_id_array[*inout_count_p] = tmp_element->id;
	if (returned_user_ptr_array)
	{
	    returned_user_ptr_array[*inout_count_p] = tmp_element->user_ptr;
	}
	out_status_array[*inout_count_p] = 0;
	tmp_element->state = REQ_SCHEDULED;
	(*inout_count_p)++;
	gossip_debug(GOSSIP_REQ_SCHED_DEBUG,
		     "REQ SCHED SCHEDULING, "
                     "handle: %llu, queue_element: %p\n",
		     llu(tmp_element->handle), tmp_element);
        PINT_req_sched_do_change_mode(tmp_element);
    }
	//	fprintf(stderr, "ORANGEFS PINT_req_sched_testworld leaving\n");
    if (*inout_count_p > 0)
	return (1);
    else
	return (0);
}

/* hash_handle()
 *
 * hash function for handles added to table
 *
 * returns integer offset into table
 */
static int hash_handle(
    void *handle,
    int table_size)
{
    /* TODO: update this later with a better hash function,
     * depending on what handles look like, for now just modding
     *
     */
    unsigned long tmp = 0;
    PVFS_handle *real_handle = handle;

    tmp += (*(real_handle));
    tmp = tmp % table_size;

    return ((int) tmp);
}

/* hash_handle_compare()
 *
 * performs a comparison of a hash table entro to a given key
 * (used for searching)
 *
 * returns 1 if match found, 0 otherwise
 */
static int hash_handle_compare(
    void *key,
    struct qlist_head *link)
{
    struct req_sched_list *my_list;
    PVFS_handle *real_handle = key;

    my_list = qlist_entry(link, struct req_sched_list,
			  hash_link);
    if (my_list->handle == *real_handle)
    {
	return (1);
    }

    return (0);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
