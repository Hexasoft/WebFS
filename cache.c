#include "cache.h"
#include "tools.h"
#include "webget.h"


/* URL for target */
extern char *url_path;



/* table of cache(s). Unused one have name=NULL */
Cache caches[CACHE_MAX];

/* global settings for caches */
int cache_chunksize=CACHE_BLOCK*8;  /* size of each chunk (max) */
int cache_chunks=1;                 /* number of chunks per cache */


/* initialise cache without freeing */
void cache_zero(Cache *cache) {
  int i;

  mylog("cache_zero(%p)\n", cache);
  /* cleanup connection */
  cache->connection.target = NULL;
  cache->connection.type = 0;
  cache->connection.data = NULL;
  cache->connection.idata = 0;
  /* cleanup chunks */
  for(i=0; i<CACHE_MAX_CHUNK; i++) {
    cache->chunks[i] = NULL;
  }
  /* cleanup cache itself */
  cache->name = NULL;
  cache->size = 0;
  cache->created = 0;
  cache->last_use = 0;
  cache->firstblock = NULL;
}

/* to be called first */
int cache_init() {
  int i;
  
  mylog("cache_init()\n");
  for(i=0; i<CACHE_MAX; i++) {
    cache_zero(&(caches[i]));
  }

  return(1);
}

void cache_chunk_free(Chunk *chunk) {
  if (chunk == NULL)
    return;
  if (chunk->data != NULL)
    free(chunk->data);
  free(chunk);
}

/* close a connection */
void cache_disconnect(Connection *cnx) {
  /* remove CURL connection (do nothing) */
  wget_disconnect(cnx);
  
  cnx->data = NULL;
  cnx->idata = 0;
  cnx->type = CNX_NDEF;
}

/* freed content of a cache */
void cache_free(Cache *cache) {
  int i;
  mylog("cache_free(%p)\n", cache);
  /* cache itself */
  if (cache->name != NULL)
    free(cache->name);
  if (cache->firstblock != NULL)
    free(cache->firstblock);
  /* chunks */
  for(i=0; i<CACHE_MAX_CHUNK; i++) {
    cache_chunk_free(cache->chunks[i]);
    cache->chunks[i] = NULL;
  }
  /* connection */
  cache_disconnect(&(cache->connection));
  if (cache->connection.target != NULL)
    free(cache->connection.target);
  cache_zero(cache);
}

/* cache cleanup (final, no rescue) */
int cache_fini() {
  int i;
  
  mylog("cache_fini()\n");
  for(i=0; i<CACHE_MAX; i++) {
    cache_free(&(caches[i]));
  }

  return(1);
}

/* search a cache by name */
Cache *cache_search(const char *file) {
  int i;
  
  mylog("cache_search(%s)\n", file);
  for(i=0; i<CACHE_MAX; i++) {
    if (caches[i].name == NULL)
      continue;
    if (strcmp(file, caches[i].name) == 0)
      return(&(caches[i]));
  }
  return(NULL);
}

/* find oldest cache */
Cache *cache_oldest() {
  int i, target=-1;
  unsigned int small = (unsigned int)-1;
  
  for(i=0; i<CACHE_MAX; i++) {
    if (caches[i].name != NULL) {
      if (target == -1) {
        target = i;
	small = caches[i].last_use;
      } else {
        if (caches[i].last_use < small) {
	  target = i;
	  small = caches[i].last_use;
	}
      }
    }
  }
  if (target == -1) /* should not happen! */
    return(NULL);
  return(&(caches[target]));
}

/* create a connection for given file/URL */
int cache_connect(Connection *cnx, const char *file, const char *url,
                  char *firstblock, unsigned int size) {

  char buffer[4096], *cvt;

  mylog("cache_connect(%p, %s, %s)\n", cnx, file, url);
  cvt = wget_encode(url, file);
  sprintf(buffer, "%s", cvt);
  /* create CURL connection (checks validity) */
  if (!wget_connect(buffer, cnx, firstblock, size)) {
    mylog("cache_connect: wget_connect(%s, -) failed\n", buffer);
    return(0);
  }
  mylog("cache_connect: wget_connect ok\n");
  cnx->target = strdup(buffer);
  if (cnx->target == NULL) {
    mylog("cache_connect: failed to copy target name");
    wget_disconnect(cnx); /* not needed, but... */
    return(0);
  }
  cnx->type = CNX_URL;
  cnx->data = NULL;  /* not used */
  cnx->idata = 0;    /* not used */
  
  return(1);
}

/* create a new cache for given file.
   returns true if created, false else
   can destroy an other one if no place available */
Cache *cache_create(const char *file, unsigned int size) {
  int i;
  Cache *tmp=NULL;
  
  mylog("cache_create(%s, %u)\n", file, size);
  /* search a free cache */
  for(i=0; i<CACHE_MAX; i++) {
    if (caches[i].name == NULL) {
      tmp = &(caches[i]);
      break;
    }
  }
  if (tmp == NULL) {
    /* not found. destroy the oldest cache */
    tmp = cache_oldest();
    if (tmp == NULL) {
      /* should not occur */
      return(NULL);
    }
  }
  /* initialise common cache data */
  tmp->name = strdup(file);
  tmp->created = tmp->last_use = (unsigned int)time(NULL);
  tmp->firstblock = NULL;
  for(i=0; i<CACHE_MAX_CHUNK; i++)
    tmp->chunks[i] = NULL;
  tmp->size = size;
  
  /* alocate 'firstblock' to cache_chunksize or size
     if smaller (for dl at 'connect') */
  tmp->firstblock = malloc(MIN(cache_chunksize, size));
  if (tmp->firstblock != NULL) {
     tmp->firstblocksize = MIN(cache_chunksize, size);
  }
  /* just ignore if failed (NULL) as in this case
     fistblock will be ignored */
  
  /* creation connection for this file */
  
  mylog("cache_create: connextion cache %p (cnx=%p)\n", tmp, &(tmp->connection));
  if (!cache_connect(&(tmp->connection), file,
          url_path, tmp->firstblock, MIN(cache_chunksize, size))) {
    /* destroy this cache... */
    cache_free(tmp);
    return(NULL);
  }
  
  mylog("cache_create: %p->connection = { %s, %d, %p, %d}\n", tmp,
    tmp->connection.target, tmp->connection.type,
    tmp->connection.data, tmp->connection.idata);
  
  /* ok */
  return(tmp);
}

/* destroy a cache */
int cache_destroy(const char *file) {
  Cache *tmp;
  
  tmp = cache_search(file);
  if (tmp == NULL)
    return(0);
  cache_free(tmp);
  return(1);
}

/* destroy all caches */
int cache_destroy_all() {
  int i;
  
  for(i=0; i<CACHE_MAX; i++) {
    if (caches[i].name != NULL)
      cache_destroy(caches[i].name);
  }

  return(1);
}


/* search in cache for given offset-size. returns a pointer
   to the 1st byte or NULL if not found */
char *cache_search_data(Cache *cache, unsigned int offset,
                        unsigned int size, unsigned int *rsize) {
  int i;

mylog("cache_search_data(%p, %u, %u, -)\n", cache, offset, size);
  if (cache == NULL)
    return(NULL);

  *rsize = size;

  /* first check in firstblock */
  if (cache->firstblock != NULL) {
    if (offset < cache->firstblocksize) {
      if (offset+size-1 < cache->firstblocksize) {
        return(cache->firstblock+offset);
      }
    }
  }
  /* check in chunks */
  for(i=0; i<CACHE_MAX_CHUNK; i++) {
    if (cache->chunks[i] != NULL) {
mylog("cache_search_data: testing chunk #%d (%d-%d)\n", i,
  cache->chunks[i]->off_start, cache->chunks[i]->off_end);
      if ((offset >= cache->chunks[i]->off_start)&&
          (offset <= cache->chunks[i]->off_end)) {
mylog("cache_search_data: %u+%u-1 (=%u) < %u\n", offset, size,
     offset+size-1, cache->chunks[i]->off_end);
        if (offset+size-1 < cache->chunks[i]->off_end) {
	  /* return data */
	  return(cache->chunks[i]->data + (offset-cache->chunks[i]->off_start));
	} else {
	  /* we have a *part* of the requested data. give it */
	  /* real size */
	  *rsize = cache->chunks[i]->off_end - offset + 1;
	  return(cache->chunks[i]->data + (offset-cache->chunks[i]->off_start));
	}
      }
    }
  }
  /* not found */
  return(NULL);
}


/* fill given chunk in given cache */
int cache_do_read(Cache *cache, int nchunk) {
  int ret;
  
  mylog("cache_do_read(%p, %d). My cnx=%p\n", cache, nchunk, &(cache->connection));

  /*
  ret = pread(fd, cache->chunks[nchunk]->data,
              cache->chunks[nchunk]->off_end-cache->chunks[nchunk]->off_start+1,
	      cache->chunks[nchunk]->off_start);
  */
  ret = wget_read(&(cache->connection),
              cache->chunks[nchunk]->off_start,
	      cache->chunks[nchunk]->off_end-cache->chunks[nchunk]->off_start+1,
	      (void*)cache->chunks[nchunk]->data);

  mylog("cache_do_read: wget_read(%d, %p, %u, %u) = %d\n", 0,
     cache->chunks[nchunk]->data,
     cache->chunks[nchunk]->off_end-cache->chunks[nchunk]->off_start+1,
     cache->chunks[nchunk]->off_start, ret);
  if (ret <= 0)
    return(0);
  return(1);
}

/* fetch data from target, from given offset */
int cache_fetch_data(Cache *cache, unsigned int offset) {
  int i, nb=-1;
  unsigned int min;


  mylog("cache_fetch_data(%p, %u)\n", cache, offset);
  /* search for a free chunk */
  for(i=0; i<cache_chunks; i++) {
    if (cache->chunks[i] == NULL) {
      nb = i;
      break;
    }
  }
  
  mylog("cache_fetch: found empty chunk slot %d\n", nb);
  
  if (nb < 0) {
    /* none free? use the smallest offset (assume seq. read - should use older) */
    min = cache->chunks[0]->off_start;
    nb = 0;
    for(i=1; i<cache_chunks; i++) {
      if (cache->chunks[i]->off_start < min) {
	min = cache->chunks[i]->off_start;
	nb = i;
      }
    }
  } else {
    /* allocate the chunk */
    cache->chunks[nb] = malloc(sizeof(Chunk));
    if (cache->chunks[nb] == NULL) {
      return(0);
    }
    cache->chunks[nb]->off_start = 0;
    cache->chunks[nb]->off_end = 0;
    cache->chunks[nb]->data = malloc(cache_chunksize);
    if (cache->chunks[nb]->data == NULL) {
      free(cache->chunks[nb]);
      cache->chunks[nb] = NULL;
      return(0);
    }
    
  mylog("cache_fetch: chunk %d allocated (size=%d) [%p-%p[\n", nb, cache_chunksize,
          cache->chunks[nb]->data, cache->chunks[nb]->data+cache_chunksize);
  }
  
  /* set new values */
  cache->chunks[nb]->off_start = offset;
  mylog("cache_fetch: put start=%u\n", offset);
  if (offset+cache_chunksize > cache->size) {
    /* after end of file. reduce */
    cache->chunks[nb]->off_end = cache->size - 1;
  } else {
    cache->chunks[nb]->off_end = offset + cache_chunksize - 1;
  }
  mylog("cache_fetch: put end=%u\n", cache->chunks[nb]->off_end);
  
  /* perform read */
  mylog("cache_fetch: performing do_read (%p, %d)\n", cache, nb);
  if (!cache_do_read(cache, nb)) {
    /* argl. this chunk is no more valid. destroy it */
    cache_chunk_free(cache->chunks[nb]);
    cache->chunks[nb] = NULL;
    return(0);
  }
  
  return(1);
}

/* read data for file in cache. data is directly put in 'dest', which
   *must* be allocated
   returns the number of bytes moved (can be less that requested in
   end of file reached and requester does not care...) */
int cache_read(unsigned int fsize, const char *file, unsigned int offset,
               unsigned int size, char *dest) {
  Cache *cache;
  char *data;
  unsigned int rsize;
  
  
  mylog("cache_read(%u, %s, %u, %u, %p)\n", fsize, file, offset, size, dest);
  if (size == 0)
    return(0);
  
  /* check for "out-of-bound" */
  if (offset >= fsize)
    return(0);  /* request is after end of file */
  
  /* search cache */
  cache = cache_search(file);
  
  mylog("cache_read: found (or not) cache %p for %s\n", cache, file);
  if (cache == NULL) {
    /* create one */
    cache = cache_create(file, fsize);
    
  mylog("cache_read: created cache %p\n");
    if (cache == NULL) {
      /* should not happen */
      return(-EBUSY);
    }
  }
  
  /* update last access */
  cache->last_use = (unsigned int)time(NULL);
  
  /* cut size if too big */
  if (offset+size > cache->size) {
  mylog("cache_read: end after EOF. Trunking. %u + %u > %u\n", offset, size, cache->size);
    size -= offset+size - cache->size;
  }
  
  /* check if data is in cache */
  data = cache_search_data(cache, offset, size, &rsize);
  mylog("cache_read: (1) cache_search_data(%p, %u, %u, -) returns %p (%u)\n",
        cache, offset, size, data, rsize);
  if (data != NULL) {
    /* copy data */
    
  mylog("cache_read: memcpy(%p, %p, %u)\n", dest, data, rsize);
    memcpy(dest, data, rsize);
    return(rsize);
  }
  
  /* no match. we need to fetch data in cache before */
  mylog("cache_read: cache_fetch(%p, %u)\n", cache, offset);
  if (!cache_fetch_data(cache, offset)) {
    mylog("cache_read: cache_fetch failed!\n");
    return(-EBUSY);
  }
  
  /* re-search for the data */
  data = cache_search_data(cache, offset, size, &rsize);
  mylog("cache_read: (2) cache_search_data(%p, %u, %u, -) returns %p (%u)\n",
        cache, offset, size, data, rsize);
  if (data != NULL) {
    /* copy data */
  mylog("cache_read: memcpy(%p, %p, %u)\n", dest, data, rsize);
    memcpy(dest, data, rsize);
    return(rsize);
  }
  /* hmmm... something is very bad... */
  return(-EBUSY);
}


