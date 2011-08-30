#include "tools.h"


int mylog_clean() {
  FILE *f;
  f = fopen("/tmp/webfs.log", "w");
  if (f != NULL)
    fclose(f);
  return(1);
}

int mylog(const char *fmt, ...) {
  FILE *f;
  int ret;
  va_list ap;
return(0);
  f = fopen("/tmp/webfs.log", "a");
  if (f == NULL)
    return(0);

  va_start(ap, fmt);
  ret = vfprintf(f, fmt, ap);
  va_end(ap);

  fclose(f);
  return(ret);
}

/* return a hash using given string */
unsigned int str_hash(const char *file) {
  unsigned int val=0;
  int i, j, dec=1;
  
  j = 1;
  for(i=strlen(file)-1; i>=0; i--) {
    val += (unsigned int)file[i]*dec;
    if (j >= 7) {
      dec = 1;
      j = 1;
    } else {
      dec *= 17;
      j++;
    }
  }

  return(val);
}

