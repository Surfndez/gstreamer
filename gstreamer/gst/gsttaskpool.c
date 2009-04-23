/* GStreamer
 * Copyright (C) 2009 Wim Taymans <wim.taymans@gmail.com>
 *
 * gsttaskpool.c: Pool for streaming threads
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gsttaskpool
 * @short_description: Pool of GStreamer streaming threads
 * @see_also: #GstTask, #GstPad
 *
 * This object provides an abstraction for creating threads. The default
 * implementation uses a regular GThreadPool to start tasks.
 *
 * Subclasses can be made to create custom threads.
 *
 * Last reviewed on 2009-04-23 (0.10.24)
 */

#include "gst_private.h"

#include "gstinfo.h"
#include "gsttaskpool.h"

GST_DEBUG_CATEGORY_STATIC (taskpool_debug);
#define GST_CAT_DEFAULT (taskpool_debug)

static void gst_task_pool_class_init (GstTaskPoolClass * klass);
static void gst_task_pool_init (GstTaskPool * pool);
static void gst_task_pool_finalize (GObject * object);

#define _do_init \
{ \
  GST_DEBUG_CATEGORY_INIT (taskpool_debug, "taskpool", 0, "Thread pool"); \
}

G_DEFINE_TYPE_WITH_CODE (GstTaskPool, gst_task_pool, GST_TYPE_OBJECT, _do_init);

static void
default_prepare (GstTaskPool * pool, GFunc func, gpointer user_data,
    GError ** error)
{
  GST_OBJECT_LOCK (pool);
  pool->pool = g_thread_pool_new ((GFunc) func, user_data, -1, FALSE, NULL);
  GST_OBJECT_UNLOCK (pool);
}

static void
default_cleanup (GstTaskPool * pool)
{
  GST_OBJECT_LOCK (pool);
  if (pool->pool) {
    /* Shut down all the threads, we still process the ones scheduled
     * because the unref happens in the thread function.
     * Also wait for currently running ones to finish. */
    g_thread_pool_free (pool->pool, FALSE, TRUE);
    pool->pool = NULL;
  }
  GST_OBJECT_UNLOCK (pool);
}

static GThread *
default_push (GstTaskPool * pool, gpointer data, GError ** error)
{
  GST_OBJECT_LOCK (pool);
  if (pool->pool)
    g_thread_pool_push (pool->pool, data, error);
  GST_OBJECT_UNLOCK (pool);

  return NULL;
}

static void
default_join (GstTaskPool * pool, GThread * thread)
{
  /* does nothing, we can't join for threads from the threadpool */
}

static void
gst_task_pool_class_init (GstTaskPoolClass * klass)
{
  GObjectClass *gobject_class;
  GstTaskPoolClass *gsttaskpool_class;

  gobject_class = (GObjectClass *) klass;
  gsttaskpool_class = (GstTaskPoolClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_task_pool_finalize);

  gsttaskpool_class->prepare = default_prepare;
  gsttaskpool_class->cleanup = default_cleanup;
  gsttaskpool_class->push = default_push;
  gsttaskpool_class->join = default_join;
}

static void
gst_task_pool_init (GstTaskPool * pool)
{
}

static void
gst_task_pool_finalize (GObject * object)
{
  GstTaskPool *pool = GST_TASK_POOL (object);

  GST_DEBUG ("taskpool %p finalize", pool);

  G_OBJECT_CLASS (gst_task_pool_parent_class)->finalize (object);
}

/**
 * gst_task_pool_new:
 *
 * Create a new default task pool. The default task pool will use a regular
 * GThreadPool for threads.
 *
 * Returns: a new #GstTaskPool. gst_object_unref() after usage.
 */
GstTaskPool *
gst_task_pool_new (void)
{
  GstTaskPool *pool;

  pool = g_object_new (GST_TYPE_TASK_POOL, NULL);

  return pool;
}

void
gst_task_pool_set_func (GstTaskPool * pool, GFunc func, gpointer user_data)
{
  g_return_if_fail (GST_IS_TASK_POOL (pool));

  pool->func = func;
  pool->user_data = user_data;
}


/**
 * gst_task_pool_prepare:
 * @pool: a #GstTaskPool
 * @error: an error return location
 *
 * Prepare the taskpool for accepting gst_task_pool_push() operations.
 *
 * MT safe.
 */
void
gst_task_pool_prepare (GstTaskPool * pool, GError ** error)
{
  GstTaskPoolClass *klass;

  g_return_if_fail (GST_IS_TASK_POOL (pool));

  klass = GST_TASK_POOL_GET_CLASS (pool);

  if (klass->prepare)
    klass->prepare (pool, pool->func, pool->user_data, error);
}

/**
 * gst_task_pool_cleanup:
 * @pool: a #GstTaskPool
 *
 * Wait for all tasks to be stopped. This is mainly used internally
 * to ensure proper cleanup of internal data structures in test suites.
 *
 * MT safe.
 */
void
gst_task_pool_cleanup (GstTaskPool * pool)
{
  GstTaskPoolClass *klass;

  g_return_if_fail (GST_IS_TASK_POOL (pool));

  klass = GST_TASK_POOL_GET_CLASS (pool);

  if (klass->cleanup)
    klass->cleanup (pool);
}

/**
 * gst_task_pool_push:
 * @pool: a #GstTaskPool
 * @data: data to pass to the thread function
 * @error: return location for an error
 *
 * Start the execution of a new thread from @pool.
 *
 * Returns: a #GThread or NULL when the GThread is not yet known. You must check
 * @error to detect errors.
 */
GThread *
gst_task_pool_push (GstTaskPool * pool, gpointer data, GError ** error)
{
  GstTaskPoolClass *klass;

  g_return_val_if_fail (GST_IS_TASK_POOL (pool), NULL);

  klass = GST_TASK_POOL_GET_CLASS (pool);

  if (klass->push == NULL)
    goto not_supported;

  return klass->push (pool, data, error);

  /* ERRORS */
not_supported:
  {
    g_warning ("pushing tasks on pool %p is not supported", pool);
    return NULL;
  }
}

/**
 * gst_task_pool_join:
 * @pool: a #GstTaskPool
 * @thread: a #GThread
 *
 * Join a GThread or return it to the pool.
 */
void
gst_task_pool_join (GstTaskPool * pool, GThread * thread)
{
  GstTaskPoolClass *klass;

  g_return_if_fail (GST_IS_TASK_POOL (pool));

  klass = GST_TASK_POOL_GET_CLASS (pool);

  if (klass->join)
    klass->join (pool, thread);
}
