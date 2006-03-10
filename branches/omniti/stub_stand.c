/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)stub_stand.c	1.3	05/06/08 SMI"

/*
 * Stubs for the standalone to reduce the dependence on external libraries
 */

#include "config.h"
#include <string.h>
#include <thread.h>

#include "misc.h"

/*ARGSUSED*/
int
_cond_init(cond_t *cvp, int type, void *arg)
{
	return (0);
}

/*ARGSUSED*/
int
_cond_destroy(cond_t *cvp)
{
	return (0);
}

/*ARGSUSED*/
int
_cond_wait(cond_t *cv, mutex_t *mutex)
{
	umem_panic("attempt to wait on standumem cv %p", cv);

	/*NOTREACHED*/
	return (0);
}

/*ARGSUSED*/
int
_cond_broadcast(cond_t *cvp)
{
	return (0);
}

thread_t
_thr_self(void)
{
	return ((thread_t)1);
}

static mutex_t _mp = DEFAULTMUTEX;

/*ARGSUSED*/
int
__mutex_init(mutex_t *mp, int type, void *arg)
{
	(void) memcpy(mp, &_mp, sizeof (mutex_t));
	return (0);
}

/*ARGSUSED*/
int
__mutex_destroy(mutex_t *mp)
{
	return (0);
}

/*ARGSUSED*/
int
__mutex_held(mutex_t *mp)
{
	return (1);
}

/*ARGSUSED*/
int
__mutex_lock(mutex_t *mp)
{
	return (0);
}

/*ARGSUSED*/
int
__mutex_trylock(mutex_t *mp)
{
	return (0);
}

/*ARGSUSED*/
int
__mutex_unlock(mutex_t *mp)
{
	return (0);
}

int
issetugid(void)
{
	return (1);
}
