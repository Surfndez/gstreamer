/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: solaris thread system implementation
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include <thread.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define solaris_print_error( name, num )				\
  g_error( "file %s: line %d (%s): error %s during %s",			\
           __FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION,			\
           g_strerror((num)), #name )

#define solaris_check_for_error( what ) G_STMT_START{			\
  int error = (what);							\
  if( error ) { solaris_print_error( what, error ); }			\
  }G_STMT_END

gulong g_thread_min_stack_size = 0;

#define HAVE_G_THREAD_IMPL_INIT
static void 
g_thread_impl_init()
{
  g_thread_min_priority = 0;
  g_thread_max_priority = 127;
#ifdef _SC_THREAD_STACK_MIN
  g_thread_min_stack_size = MAX (sysconf (_SC_THREAD_STACK_MIN), 0);
#endif /* _SC_THREAD_STACK_MIN */
}

static GMutex *
g_mutex_new_solaris_impl (void)
{
  GMutex *result = (GMutex *) g_new (mutex_t, 1);
  solaris_check_for_error (mutex_init ((mutex_t *) result, USYNC_THREAD, 0));
  return result;
}

static void
g_mutex_free_solaris_impl (GMutex * mutex)
{
  solaris_check_for_error (mutex_destroy ((mutex_t *) mutex));
  free (mutex);
}

/* NOTE: the functions g_mutex_lock and g_mutex_unlock may not use
   functions from gmem.c and gmessages.c; */

/* mutex_lock, mutex_unlock can be taken directly, as
   signature and semantic are right, but without error check then!!!!,
   we might want to change this therefore. */

static gboolean
g_mutex_trylock_solaris_impl (GMutex * mutex)
{
  int result;
  result = mutex_trylock ((mutex_t *) mutex);
  if (result == EBUSY)
    return FALSE;
  solaris_check_for_error (result);
  return TRUE;
}

static GCond *
g_cond_new_solaris_impl ()
{
  GCond *result = (GCond *) g_new (cond_t, 1);
  solaris_check_for_error (cond_init ((cond_t *) result, USYNC_THREAD, 0));
  return result;
}

/* cond_signal, cond_broadcast and cond_wait
   can be taken directly, as signature and semantic are right, but
   without error check then!!!!, we might want to change this
   therfore. */

#define G_NSEC_PER_SEC 1000000000

static gboolean
g_cond_timed_wait_solaris_impl (GCond * cond, 
				GMutex * entered_mutex,
				GTimeVal * abs_time)
{
  int result;
  timestruc_t end_time;
  gboolean timed_out;

  g_return_val_if_fail (cond != NULL, FALSE);
  g_return_val_if_fail (entered_mutex != NULL, FALSE);

  if (!abs_time)
    {
      g_cond_wait (cond, entered_mutex);
      return TRUE;
    }

  end_time.tv_sec = abs_time->tv_sec;
  end_time.tv_nsec = abs_time->tv_usec * (G_NSEC_PER_SEC / G_USEC_PER_SEC);
  g_assert (end_time.tv_nsec < G_NSEC_PER_SEC);
  result = cond_timedwait ((cond_t *) cond, (mutex_t *) entered_mutex,
			   &end_time);
  timed_out = (result == ETIME);
  if (!timed_out)
    solaris_check_for_error (result);
  return !timed_out;
}

static void
g_cond_free_solaris_impl (GCond * cond)
{
  solaris_check_for_error (cond_destroy ((cond_t *) cond));
  g_free (cond);
}

static GPrivate *
g_private_new_solaris_impl (GDestroyNotify destructor)
{
  GPrivate *result = (GPrivate *) g_new (thread_key_t,1);
  solaris_check_for_error (thr_keycreate ((thread_key_t *) result,
					  destructor));
  return result;
}

/* NOTE: the functions g_private_get and g_private_set may not use
   functions from gmem.c and gmessages.c */

static void
g_private_set_solaris_impl (GPrivate * private_key, gpointer value)
{
  if (!private_key)
    return;

  thr_setspecific (*(thread_key_t *) private_key, value);
}

static gpointer
g_private_get_solaris_impl (GPrivate * private_key)
{
  gpointer result;

  if (!private_key)
    return NULL;
  
  thr_getspecific (*(thread_key_t *) private_key, &result);

  return result;
}

static void
g_thread_set_priority_solaris_impl (gpointer thread, GThreadPriority priority)
{
  solaris_check_for_error (thr_setprio (*(thread_t*)thread,  
					g_thread_map_priority (priority)));
}

static void
g_thread_create_solaris_impl (GThreadFunc thread_func, 
			      gpointer arg, 
			      gulong stack_size,
			      gboolean joinable,
			      gboolean bound,
			      GThreadPriority priority,
			      gpointer thread,
			      GError **error)
{     
  long flags = (bound ? THR_BOUND : 0) | (joinable ? 0: THR_DETACHED);
  gint ret;
  
  g_return_if_fail (thread_func);
  
  stack_size = MAX (g_thread_min_stack_size, stack_size);
  
  ret = thr_create (NULL, stack_size, (void* (*)(void*))thread_func,
		    arg, flags, thread);

  if (ret == EAGAIN)
    {
      g_set_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN, 
		   "Error creating thread: %s", g_strerror (ret));
      return;
    }
  
  solaris_check_for_error (ret);

  g_thread_set_priority_solaris_impl (thread, priority);
}

static void 
g_thread_yield_solaris_impl (void)
{
  thr_yield ();
}

static void
g_thread_join_solaris_impl (gpointer thread)
{     
  gpointer ignore;
  solaris_check_for_error (thr_join (*(thread_t*)thread, NULL, &ignore));
}

static void 
g_thread_exit_solaris_impl (void) 
{
  thr_exit (NULL);
}

static void
g_thread_self_solaris_impl (gpointer thread)
{
  *(thread_t*)thread = thr_self();
}

static GThreadFunctions g_thread_functions_for_glib_use_default =
{
  g_mutex_new_solaris_impl,
  (void (*)(GMutex *)) mutex_lock,
  g_mutex_trylock_solaris_impl,
  (void (*)(GMutex *)) mutex_unlock,
  g_mutex_free_solaris_impl,
  g_cond_new_solaris_impl,
  (void (*)(GCond *)) cond_signal,
  (void (*)(GCond *)) cond_broadcast,
  (void (*)(GCond *, GMutex *)) cond_wait,
  g_cond_timed_wait_solaris_impl,
  g_cond_free_solaris_impl,
  g_private_new_solaris_impl,
  g_private_get_solaris_impl,
  g_private_set_solaris_impl,
  g_thread_create_solaris_impl,
  g_thread_yield_solaris_impl,
  g_thread_join_solaris_impl,
  g_thread_exit_solaris_impl,
  g_thread_set_priority_solaris_impl,
  g_thread_self_solaris_impl
};
