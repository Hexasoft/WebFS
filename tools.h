#ifndef __tools_h_
#define __tools_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>


/* global settings */
extern int opt_exec_files;


/* various */
#define MIN(a,b) ((a)<(b)?(a):(b))


/* logs */
/* reset log file */
int mylog_clean();
/* print into logfile */
int mylog(const char *fmt, ...);

/* return a hash using given string */
unsigned int str_hash(const char *file);


#endif /* __tools_h_ */
