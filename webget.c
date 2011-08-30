#include "webget.h"
#include "tools.h"
#include "cache.h"


CURL *wget_handler = NULL;


/* initialise CURL stuff */
int wget_init() {

  curl_global_init(CURL_GLOBAL_ALL);
  wget_handler = curl_easy_init();
  
  if (wget_handler == NULL)
    return(0);
  
  /* init */
  curl_easy_setopt(wget_handler, CURLOPT_USERAGENT, "libcurl-WebFS/1.0");
  curl_easy_setopt(wget_handler, CURLOPT_URL, NULL);
  curl_easy_setopt(wget_handler, CURLOPT_RANGE, NULL);
  curl_easy_setopt(wget_handler, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(wget_handler, CURLOPT_WRITEDATA, NULL);
  
  return(1);
}

/* terminate CURL stuff */
int wget_fini() {
  curl_easy_cleanup(wget_handler);
  
  return(1);
}

/* the "data-copy" function */
size_t wget_push_data(void *ptr, size_t size, size_t nmemb, void *data) {
  static unsigned int offset = 0;
  mylog("wget_push_data: asked to copy %u*%u=%u bytes from WEB link\n",
        (unsigned int)size, (unsigned int)nmemb, (unsigned int)(size*nmemb));

  /* special: reset offset */
  if ((ptr == NULL)&&(data == NULL)) {
    offset = 0;
    return(0);
  }

  if (data == NULL) {
    /* just ignore write */
    mylog("wget_push_data: data is NULL. Skip.\n");
    offset = 0;
    return(size*nmemb);
  }
  mylog("wget_push_data: memcpy(%p+%u=%p, %p, %u)\n", data, offset, data+offset, ptr,
         (unsigned int)size*nmemb);
  memcpy(data+offset, ptr, size*nmemb);
  offset += size*nmemb;
  return(size*nmemb);
}


/* create a CURL handler for the given URL.
   CURL must be previously initialised.
   get an anonymous pointer to store  */
int wget_connect(char *url, Connection *cnx, char *fistblock, unsigned int size) {
  CURLcode r;
  long reply;
  char buffer[64];

  mylog("wget_connect(%s, %p)\n", url, cnx);
  mylog("wget_connect: handler = %p\n", wget_handler);

  /* check that file exists */
  curl_easy_setopt(wget_handler, CURLOPT_URL, url);
  /* we need nothing, just testing the connection */
  curl_easy_setopt(wget_handler, CURLOPT_HEADER, 0L);
  curl_easy_setopt(wget_handler, CURLOPT_WRITEFUNCTION, wget_push_data);
  /* if 'fistblock' is NULL, to not request for file content, else
     set options for that */
  if (fistblock == NULL) {
    curl_easy_setopt(wget_handler, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(wget_handler, CURLOPT_NOBODY, 1L);
  } else {
    /* reset offset */
    wget_push_data(NULL, 0, 0, NULL);
    curl_easy_setopt(wget_handler, CURLOPT_WRITEDATA, fistblock);
    curl_easy_setopt(wget_handler, CURLOPT_NOBODY, 0L);
    sprintf(buffer, "%u-%u", 0, size-1);
    curl_easy_setopt(wget_handler, CURLOPT_RANGE, buffer);
  }
  /* perform request */
  mylog("wget_connect: performing request...\n");
  r = curl_easy_perform(wget_handler);
  mylog("wget_connect: done\n");
  if (r != CURLE_OK) {
    mylog("wget_connect: perform returned %d\n", r);
    curl_easy_setopt(wget_handler, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(wget_handler, CURLOPT_HEADER, 0L);
    curl_easy_setopt(wget_handler, CURLOPT_WRITEDATA, NULL);
    return(0);
  }
  curl_easy_setopt(wget_handler, CURLOPT_NOBODY, 0L);
  curl_easy_setopt(wget_handler, CURLOPT_HEADER, 0L);
  curl_easy_setopt(wget_handler, CURLOPT_WRITEDATA, NULL);

  /* check return value */
  mylog("wget_connect: checking status...\n");
  curl_easy_getinfo(wget_handler, CURLINFO_RESPONSE_CODE , &reply);
  if (reply == 404)
    return(0);  /* does not exists */
  
  return(1);
}

/* remove a CURL handler from CURL */
int wget_disconnect(Connection *cnx) {
  /* nothing to do */
  return(1);
}


/* perform affective read from existing handler */
int wget_read(Connection *cnx, unsigned int offset, unsigned int size,
              char *dest) {
  char buffer[64];
  CURLcode r;
  char *cvt = NULL;

  /* set data */
  curl_easy_setopt(wget_handler, CURLOPT_URL, cnx->target);
  sprintf(buffer, "%u-%u", offset, offset+size-1);
  curl_easy_setopt(wget_handler, CURLOPT_RANGE, buffer);
  mylog("wget_read: set options to '%s' [%s]\n", cnx->target, buffer);
  /* our output function */
  curl_easy_setopt(wget_handler, CURLOPT_WRITEFUNCTION, wget_push_data);
  curl_easy_setopt(wget_handler, CURLOPT_WRITEDATA, dest);
  curl_easy_setopt(wget_handler, CURLOPT_HEADER, 0L);
  /* initialise local offset */
  wget_push_data(NULL, 0, 0, NULL);

  /* perform read */
  r = curl_easy_perform(wget_handler);

  /* remove options */
  curl_easy_setopt(wget_handler, CURLOPT_RANGE, NULL);
  curl_easy_setopt(wget_handler, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(wget_handler, CURLOPT_WRITEDATA, NULL);
  curl_easy_setopt(wget_handler, CURLOPT_HEADER, 1L);

  /* error? */
  if (r != CURLE_OK)
    return(-ENOTCONN);
  
  return((int)(size));
}

/* get the FS description file in local */
int wget_meta(char *url, FILE *f) {
  CURL *tmp;
  CURLcode r;
  char *cvt = NULL;
printf("aaa\n");
  tmp = curl_easy_init();
  if (tmp == NULL)
    return(0);
printf("bbb '%s'\n", url);
  /* set data */
  /* url *must* be pre-encoded */
  curl_easy_setopt(tmp, CURLOPT_URL, url);
  curl_easy_setopt(tmp, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(tmp, CURLOPT_WRITEDATA, f);
  curl_easy_setopt(tmp, CURLOPT_HEADER, 0L);

  r = curl_easy_perform(tmp);
  if (r != CURLE_OK) {
printf("beuah!\n");
    return(0);
  }

  curl_easy_cleanup(tmp);

  return(1);
}


void push_converted_char(const char c, char *buffer) {
  char tmp[16];
  
  tmp[0] = '%';
  sprintf(tmp+1, "%X", (int)c);
  strcat(buffer, tmp);
}

void push_normal_char(const char c, char *buffer) {
  char tmp[4];
  
  tmp[0] = c;
  tmp[1] = '\0';
  strcat(buffer, tmp);
}

int char_need_convert(const char c) {
  if ((c >= 'a')&&(c <= 'z'))
    return(0);
  if ((c >= 'A')&&(c <= 'Z'))
    return(0);
  if ((c >= '0')&&(c <= '9'))
    return(0);
  if ((c == '_')||(c == '.')||(c == '/'))
    return(0);
  return(1);
}

char *wget_encode(const char *base, const char *url) {
  static char buffer[8192];
  char *cvt;
  int i;
  
  buffer[0] = '\0';
  strcat(buffer, base);
  if (url[0] != '/')
    strcat(buffer, "/");
  for(i=0; i<strlen(url); i++) {
    if (char_need_convert(url[i]))
      push_converted_char(url[i], buffer);
    else
      push_normal_char(url[i], buffer);
  }
  return(buffer);
}
