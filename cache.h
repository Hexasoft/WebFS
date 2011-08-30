#ifndef __cache_h_
#define __cache_h_

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>



/* this size is the data-content of a HTTP download.
    I use this value *8 because it is a little greater
    than the one used by 'file', so that a chunk is always
    suitable for common file operations */
#define CACHE_BLOCK 16090

/* global settings for caches */
#define CACHE_MAX_CHUNK 8    /* number of chunk is <= to this value */
extern int cache_chunksize;  /* size of each chunk (max) */
extern int cache_chunks;     /* number of chunks per cache */


/* type of connection */
#define CNX_NDEF 0
#define CNX_URL  1
#define CNX_FILE 2  /* this one is for test purpose */

/* structure of a "connection" (URL or FILE) */
typedef struct {
  char *target;  /* file name (full) or URL */
  int type;      /* _URL or _FILE */
  void *data;    /* data for handler (i.e. CURL) */
  int idata;     /* same */
}Connection;

/* TODO:
   configuration should allow to deal with multiple webservers
   so the cache and/or the main program should be aware of what
   part of data are requested, and what URL should be used for
   that (and maybe which CURL options...)
*/

/* structure of a cache chunk */
typedef struct {
  unsigned int off_start;  /* offset of 1st byte in cache */
  unsigned int off_end;    /* offset of last byte in cache */
  char *data;              /* data in cache, size=last-first+1 */
}Chunk;

/* structure of a cache */
typedef struct {
  /* connection */
  Connection connection;
  /* informations about file */
  char *name;         /* full path for local file */
  unsigned int size;  /* total size */
  /* informations about the cache */
  unsigned int created;  /* creation timestamp */
  unsigned int last_use; /* last access timestamp */
  /* first-block cache (4096) */
  char *firstblock;   /* allocated at first request, if used in a read
    (copy of chunk content) */
  unsigned int firstblocksize;
  Chunk *chunks[CACHE_MAX_CHUNK]; /* list of pointers to chunks (or NULL) */
}Cache;


#define CACHE_MAX 16  /* max 16 simultaneous caches */

/* table of cache(s). Unused one have name=NULL */
extern Cache caches[CACHE_MAX];



/** functions **/

/* to be called first */
int cache_init();

/* to be called last. destroy all caches */
int cache_fini();

/* freed content of a cache */
void cache_free(Cache *cache);

/* create a new cache for given file.
   returns true if created, false else
   can destroy an other one if no place available */
Cache *cache_create(const char *file, unsigned int size);

/* destroy a cache */
int cache_destroy(const char *file);

/* destroy a cache */
int cache_destroy_all();

/* read data for file in cache. data is directly put in 'dest', which
   *must* be allocated
   returns the number of bytes moved (can be less that requested in
   end of file reached and requester does not care...) */
int cache_read(unsigned int fsize, const char *file, unsigned int offset,
               unsigned int size,  char *dest);


#endif /* __cache_h_ */

