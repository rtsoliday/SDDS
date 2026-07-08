/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * rpnlib keeps definitions, memories, and arrays in process-wide globals,
 * while execution stacks are per-thread. This recursive lock serializes
 * library entry points while still allowing nested RPN evaluation from
 * commands such as simpson integration.
 */
#include "rpn_internal.h"

static MDB_THREAD_LOCK rpn_mutex = MDB_THREAD_LOCK_INITIALIZER;
static RPN_THREAD_LOCAL long rpn_lock_depth = 0;

void rpn_lock(void)
{
  if (rpn_lock_depth == 0)
    mdb_thread_lock(&rpn_mutex);
  rpn_lock_depth++;
}

void rpn_unlock(void)
{
  if (rpn_lock_depth <= 0)
    return;
  rpn_lock_depth--;
  if (rpn_lock_depth == 0) {
    rpn_update_legacy_stackptrs();
    mdb_thread_unlock(&rpn_mutex);
  }
}
