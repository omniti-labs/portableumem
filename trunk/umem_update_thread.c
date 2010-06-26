/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Portions Copyright 2006-2008 Message Systems, Inc.
 */

/* #pragma ident	"@(#)umem_update_thread.c	1.2	05/06/08 SMI" */

#include "config.h"
#include "umem_base.h"
#include "vmem_base.h"

#include <signal.h>

struct umem_suspend_signal_object {
	/* locked by creating thread; unlocked when umem_update_thread
	 * can proceed */
	pthread_mutex_t mtx;
	/* lock associated with the condition variable */
	pthread_mutex_t cmtx;
	/* condition variable is signalled by umem_update_thread when
	 * it has obtained the mtx; it is then safe for the creating
	 * thread to clean up its stack (on which this object resides) */
	pthread_cond_t cond;
	int flag;
};

/*ARGSUSED*/
static THR_RETURN
THR_API umem_update_thread(void *arg)
{
	struct timeval now;
	int in_update = 0;
	struct umem_suspend_signal_object *obj = arg;

	pthread_mutex_lock(&obj->mtx);
	obj->flag = 1;
	pthread_cond_signal(&obj->cond);
	obj = NULL;

	(void) mutex_lock(&umem_update_lock);

	ASSERT(umem_update_thr == thr_self());
	ASSERT(umem_st_update_thr == 0);

	for (;;) {
		umem_process_updates();

		if (in_update) {
			in_update = 0;
			/*
			 * we wait until now to set the next update time
			 * so that the updates are self-throttling
			 */
			(void) gettimeofday(&umem_update_next, NULL);
			umem_update_next.tv_sec += umem_reap_interval;
		}

		switch (umem_reaping) {
		case UMEM_REAP_DONE:
		case UMEM_REAP_ADDING:
			break;

		case UMEM_REAP_ACTIVE:
			umem_reap_next = gethrtime() +
			    (hrtime_t)umem_reap_interval * NANOSEC;
			umem_reaping = UMEM_REAP_DONE;
			break;

		default:
			ASSERT(umem_reaping == UMEM_REAP_DONE ||
			    umem_reaping == UMEM_REAP_ADDING ||
			    umem_reaping == UMEM_REAP_ACTIVE);
			break;
		}

		(void) gettimeofday(&now, NULL);
		if (now.tv_sec > umem_update_next.tv_sec ||
		    (now.tv_sec == umem_update_next.tv_sec &&
		    now.tv_usec >= umem_update_next.tv_usec)) {
			/*
			 * Time to run an update
			 */
			(void) mutex_unlock(&umem_update_lock);

			vmem_update(NULL);
			/*
			 * umem_cache_update can use umem_add_update to
			 * request further work.  The update is not complete
			 * until all such work is finished.
			 */
			umem_cache_applyall(umem_cache_update);

			(void) mutex_lock(&umem_update_lock);
			in_update = 1;
			continue;	/* start processing immediately */
		}

		/*
		 * if there is no work to do, we wait until it is time for
		 * next update, or someone wakes us.
		 */
		if (umem_null_cache.cache_unext == &umem_null_cache) {
			int cancel_state;
			timespec_t abs_time;
			abs_time.tv_sec = umem_update_next.tv_sec;
			abs_time.tv_nsec = umem_update_next.tv_usec * 1000;

			(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
			    &cancel_state);
			(void) cond_timedwait(&umem_update_cv,
			    &umem_update_lock, &abs_time);
			(void) pthread_setcancelstate(cancel_state, NULL);
		}
	}
	/* LINTED no return statement */
}

int
umem_create_update_thread(void)
{
#ifndef _WIN32
	sigset_t sigmask, oldmask;
#endif
	pthread_t newthread;
	pthread_attr_t attr;
	struct umem_suspend_signal_object obj;
	int cancel_state;

	ASSERT(MUTEX_HELD(&umem_update_lock));
	ASSERT(umem_update_thr == 0);

#ifndef _WIN32
	/*
	 * The update thread handles no signals
	 */
	(void) sigfillset(&sigmask);
	(void) pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask);
#endif
	/*
	 * drop the umem_update_lock; we cannot hold locks acquired in
	 * pre-fork handler while calling thr_create or thr_continue().
	 */

	(void) mutex_unlock(&umem_update_lock);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_mutex_init(&obj.mtx, NULL);
	pthread_mutex_init(&obj.cmtx, NULL);
	pthread_cond_init(&obj.cond, NULL);
	obj.flag = 0;
	pthread_mutex_lock(&obj.mtx);

	if (pthread_create(&newthread, &attr, umem_update_thread, &obj) == 0) {
#ifndef _WIN32
		(void) pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
#endif
		(void) mutex_lock(&umem_update_lock);
		/*
		 * due to the locking in umem_reap(), only one thread can
		 * ever call umem_create_update_thread() at a time.  This
		 * must be the case for this code to work.
		 */

		ASSERT(umem_update_thr == 0);
		umem_update_thr = newthread;
		(void) mutex_unlock(&umem_update_lock);

		/* tell the thread to continue */
		pthread_mutex_unlock(&obj.mtx);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);
		/* wait for it to be done with obj */
		pthread_mutex_lock(&obj.cmtx);
		do {
			if (obj.flag) {
				break;
			}
			ASSERT(pthread_cond_wait(&obj.cond, &obj.cmtx) == 0);
		} while (1);
		pthread_setcancelstate(cancel_state, NULL);
		pthread_mutex_destroy(&obj.mtx);
		pthread_mutex_destroy(&obj.cmtx);
		pthread_cond_destroy(&obj.cond);

		(void) mutex_lock(&umem_update_lock);

		return (1);
	} else { /* thr_create failed */
		(void) thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
		(void) mutex_lock(&umem_update_lock);
		pthread_mutex_destroy(&obj.mtx);
		pthread_mutex_destroy(&obj.cmtx);
		pthread_cond_destroy(&obj.cond);
	}
	return (0);
}
