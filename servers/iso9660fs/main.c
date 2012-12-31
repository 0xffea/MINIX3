
#include "inc.h"

#include <minix/vfsif.h>

#include <assert.h>

#include "const.h"
#include "glo.h"

/*
 * This file contains the main directory for the server. It waits for a 
 * request and then send a response.
 */

/* Declare some local functions. */
static void get_work(message *m_in);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

/*
 *
 */
int
main(void)
{
	endpoint_t who_e;
	int	ind;
	int	transid;
	int	error;

	/*
	 * SEF local startup.
	 */
	sef_local_startup();

	for (;;) {

		/* Wait for request message */
		get_work(&fs_m_in);

		transid = TRNS_GET_ID(fs_m_in.m_type);
		fs_m_in.m_type = TRNS_DEL_ID(fs_m_in.m_type);

		if (fs_m_in.m_type == 0) {
			assert(!IS_VFS_FS_TRANSID(transid));

			/* Backwards compat. */
			fs_m_in.m_type = transid;
			transid = 0;
		} else {
			assert(IS_VFS_FS_TRANSID(transid));
		}

		error = OK;

		/* To trap errors */
		caller_uid = -1;
		caller_gid = -1;

		/* Source of the request */
		who_e = fs_m_in.m_source;

		/*
		 * If the message is not for us just continue.
		 */
		if (who_e != VFS_PROC_NR) { 
			continue;
		}

		req_nr = fs_m_in.m_type;

		if (req_nr < VFS_BASE) {
			fs_m_in.m_type += VFS_BASE;
			req_nr = fs_m_in.m_type;
		}

		ind = req_nr - VFS_BASE;

		if (ind < 0 || ind >= NREQS) {
			error = EINVAL; 
		} else {
			/*
			 * Process the request calling the
			 * appropriate functions.
			 */
			error = (*fs_call_vec[ind])();
		}	

		fs_m_out.m_type = error; 
		if (IS_VFS_FS_TRANSID(transid)) {
			/* If a transaction ID was set, reset it */
			fs_m_out.m_type = TRNS_ADD_ID(fs_m_out.m_type, transid);
		}

		/*
		 * Returns the response to the VFS.
		 */
		reply(who_e, &fs_m_out);
	}
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fail);

  /* No live update support for now. */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();

  lmfs_buf_pool(10);
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the iso9660fs server. */

   /* SELF_E will contain the id of this process */
   SELF_E = getprocnr();
/*    hash_init(); */			/* Init the table with the ids */
   setenv("TZ","",1);		/* Used to calculate the time */

   return(OK);
}

/*===========================================================================*
 *				sef_cb_signal_handler			     *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* No need to do a sync, as this is a read-only file system. */

  /* If the file system has already been unmounted, exit immediately.
   * We might not get another message.
   */
  if (unmountdone) exit(0);
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
static void get_work(m_in)
message *m_in;				/* pointer to message */
{
  int s;					/* receive status */
  if (OK != (s = sef_receive(ANY, m_in))) 	/* wait for message */
    panic("sef_receive failed: %d", s);
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
void reply(who, m_out)
int who;	
message *m_out;                       	/* report result */
{
  if (OK != send(who, m_out))    /* send the message */
    printf("ISOFS(%d) was unable to send reply\n", SELF_E);
}
