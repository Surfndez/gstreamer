#include <gst/gst.h>
#include "gstmemchunk.h"

#define MAX_THREADS  100

static GstMemChunk *_chunks;

static gint num_allocs;
static gint num_threads;

static gpointer 
alloc_chunk (void)
{
  gpointer ret;
  ret = gst_mem_chunk_alloc (_chunks);

  return ret;
}

static void 
free_chunk (gpointer chunk)
{
  gst_mem_chunk_free (_chunks, chunk);
}


void*
run_test (void *threadid)
{
  gint i;
  gpointer chunk;
  sleep(1);

  for (i = 0; i<num_allocs; i++) {
    chunk = alloc_chunk ();
    free_chunk (chunk);
  }

  pthread_exit(NULL);
}


gint 
main (gint argc, gchar *argv[]) 
{
  pthread_t threads[MAX_THREADS];
  int rc, t;
 
  gst_init (&argc, &argv);

  if (argc != 3) {
    g_print ("usage: %s <num_threads> <num_allocs>\n", argv[0]);
    exit (-1);
  }

  num_threads = atoi (argv[1]);
  num_allocs = atoi (argv[2]);

  _chunks = gst_mem_chunk_new ("test", 32, 32 * 16, G_ALLOC_AND_FREE);

  for(t=0; t < num_threads; t++) {
    rc = pthread_create (&threads[t], NULL, run_test, (void *)t);
    if (rc) {
      printf ("ERROR: return code from pthread_create() is %d\n", rc);
      printf ("Code %d= %s\n", rc, strerror(rc));
      exit (-1);
    }
  }
  printf ("main(): Created %d threads.\n", t);

  for(t=0; t < num_threads; t++) {
    pthread_join (threads[t], NULL);
  }
  g_mem_chunk_info();

  gst_mem_chunk_destroy (_chunks);

  return 0;
}
