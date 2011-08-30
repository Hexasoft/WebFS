#ifndef __webget_h_
#define __webget_h_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "cache.h"


/* initialise CURL stuff */
int wget_init();

/* terminate CURL stuff */
int wget_fini();

/* create a CURL handler for the given URL.
   CURL must be previously initialised.
   get an anonymous pointer to store  */
int wget_connect(char *url, Connection *cnx, char *fistblock, unsigned int size);

/* remove a CURL handler from CURL */
int wget_disconnect(Connection *cnx);

/* the "data-copy" function */
size_t wget_push_data(void *ptr, size_t size, size_t nmemb, void *data);

/* perform affective read from existing handler */
int wget_read(Connection *cnx, unsigned int offset, unsigned int size, char *dest);


/* get the FS description file in local */
int wget_meta(char *url, FILE *f);

/* URL-encode base+file. static return */
char *wget_encode(const char *base, const char *url);


#endif /* __webget_h_ */
