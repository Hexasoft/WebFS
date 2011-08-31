/*
 * WebFS: mounts a read-only filesystem based on Web site content
 * 
 */


#define FUSE_USE_VERSION 26

static const char* webfsName = "WebFS";
static const char* webfsVersion = "0.3";
static const char *webfsDescription =
  "Show a HTTP tree as a local readonly filesystem.\n"
  "Coded by Hexasoft. Initial code inspired by rofs by Matthew Keller\n";


#define _XOPEN_SOURCE 500

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <fuse.h>
#include <stdarg.h>
#include <stddef.h>

#include "tree.h"
#include "tools.h"
#include "cache.h"
#include "webget.h"


/* URL to use */
char *url_path;

/* metadata file on HTTP server */
char *url_metadata = "/description.data";

/* the name of the temp file that contains the
   meta-data of the filesystem. used at startup
   and for updates */
char tpl[4096];
/* URL where to find metadata file on server */
char metaurl[4096];


/* last time we dl the metadata file */
unsigned int last_dl=0;

/* min. time beetween dl of metadata file */
unsigned int intv_dl=60; /* 1 minute */


/* global setting: force files to be executable */
int opt_exec_files = 0;


/* status for updates */
#define UP_OK   0  /* last update ok */
#define UP_CNX  1  /* failed to connect to web server */
#define UP_404  2  /* failed to find/get metadata file */
#define UP_TREE 3  /* failed to create tree from metadata file */
#define UP_INT  4  /* internal error */
int update_ok = 0;
int update_nbent = 0; /* # entries in metadata file */

char *update_str[] = { "Ok", "Failed to connect to web server",
   "Failed to find/get metadata file on web server",
   "Failed to create FS tree from metadata file",
   "Internal error",
   "<ndef>", NULL };

/* last detailled error message */
char update_msg[1024];


void set_message(char *msg) {
  if (msg == NULL) {
    update_msg[0] = 'O';
    update_msg[1] = 'k';
    update_msg[2] = '\0';
  } else {
    update_msg[0] = '\0';
    strcat(update_msg, msg);
  }
}


/* global statistics */
unsigned long long int stat_open = 0;  /* # open */
unsigned long long int stat_stat = 0;  /* stat or similar */
unsigned long long int stat_read = 0;  /* # read */
unsigned long long int stat_dir  = 0;  /* # readdir */
unsigned long long int stat_data = 0;  /* # bytes read */
unsigned int stat_used = 0;  /* number of files opened now */




/* get the node corresponding to path, or NULL */
static Node* get_node_path(const char* path) {
    return(tree_search(path));
}


/* load metadata file */
int load_metadata() {
  FILE *f;
  unsigned int stp;
mylog("::load_metadata()\n");
  if (metaurl[0] != '@') {
    /* not for @ code: it is not updated */
    f = fopen(tpl, "w");
    if (f == NULL) {
      update_ok = UP_INT;
      set_message("Failed to create local metadata file (check /tmp).");
      return(0);
    }
  }
  
  if (metaurl[0] == '@') {
    /* user ask for a local program to update file */
    /* run it with "system". may change */
    if (system(metaurl+1) != 0) {
      update_ok = UP_404; /* fail. same error than 404 */
      set_message("Failed to execute updater program for metadata.");
      return(0);
    }
    return(1);
  } else {
    /* we just dl the metadata file from website */
    if (!wget_meta(metaurl, f)) {
      fclose(f);
      update_ok = UP_404;
      return(0);
    }
    fclose(f);
  }
  
  /* now check for timestamp value */
  f = fopen(tpl, "r");
  if (f == NULL) {
    update_ok = UP_INT;
    set_message("Failed to open local metadata file (check /tmp).");
    return(0);
  }
  if (fscanf(f, "%u", &stp) != 1) {
    fclose(f);
    update_ok = UP_TREE;
    set_message("Failed to find timestamp in metadata file (bad format?).");
    return(0);
  }
  fclose(f);
  set_message(NULL);
  
  return(1);
}

/* this function:
  - checks if we dl metadata file for too long
  - if yes, re-download metadata file
  - if ok, check for timestamp in it
  - if more recent than current tree, update it
*/
int update_meta_if_needed() {
  unsigned int cur, act;
  int nb;
  FILE *f;


mylog("::update_meta_if_meeded()\n");
  cur = (unsigned int)time(NULL);
  if (cur < last_dl + intv_dl) {
    set_message(NULL);
    return(0);  /* too recent */
  }

  /* re-get the file */
  if (!load_metadata()) {
    /* eeek! */
    return(0);  /* how to tell to user? */
    /* done with /.status */
  }
  /* update dl time */
  last_dl = cur;
  
  /* check for timestamp in it */
  f = fopen(tpl, "r");
  if (f == NULL) {
    update_ok = UP_INT;
    set_message("Failed to open local metadata file (check /tmp).");
    return(0);
  }
  if (fscanf(f, "%u", &act) < 1) {
    fclose(f);
    update_ok = UP_INT;
    set_message("Failed to find timestamp in metadata file (bad format?).");
    return(0);
  }
  fclose(f);
  
  /* if older or same, do nothing */
  if (act <= update) {
    set_message(NULL);
    return(0);
  }
  
  /* ok, so it is newer. we need to rebuild the FS tree.
     but before we need to destroy the cache data that
     should point to current node elements
  */

mylog("::update_meta_if_meeded: timestamp newer: updating tree\n");
  cache_destroy_all();
  
  /* re-load tree */
  f = fopen(tpl, "r");
  if (f == NULL) {
    update_ok = UP_INT;
    set_message("Failed to open local metadata file (check /tmp).");
    return(0);
  }
  /* destroy & recreate */
  tree_free();
  nb = tree_create(f);
  if (nb <= 0) {
    /* this is a very bad error */
    update_ok = UP_TREE;
    set_message("Failed to read metadata file (bad format?).");
    return(0);
  }
  update_ok = UP_OK;
  update_nbent = nb;
  fclose(f);
  set_message(NULL);

  return(1);
}


/* copy/compute data for 'struct stat' from given node */
static int getattr_from_node(Node *node, struct stat *st_data) {
    /* fill the answer */
    st_data->st_ino = (ino_t)node->inode;
    st_data->st_nlink = (nlink_t)node->links;
    st_data->st_size = (off_t)node->size;
    st_data->st_atime = st_data->st_mtime = st_data->st_ctime = (time_t)node->stamp;
    st_data->st_mode = (mode_t)(node->m_other+8*node->m_grp+8*8*node->m_own);
    if (node->file) {
        /* is it a symlink? */
	if (node->symlink != NULL)
	  st_data->st_mode |= S_IFLNK; /* symlink */
	else
          st_data->st_mode |= S_IFREG; /* regular (file) */
    } else {
        st_data->st_mode |= S_IFDIR; /* directory */
    }
    st_data->st_blksize = 4096;
    st_data->st_blocks = 4096;
    st_data->st_uid = 0;
    st_data->st_gid = 0;
    st_data->st_rdev = 0;
    
    return(0);
}



/* 
 * callbacks for FUSE (various access to FS)
 */


/** may add _init_ and _destroy_, to handle creation/destruction of FS tree **/


/* perform a 'stat' on the file */
static int callback_getattr(const char *path, struct stat *st_data) {
    Node *node;
mylog("::getattr(%s)\n", path);
    node = get_node_path(path);
    if (node == NULL) {
        return(-ENOENT);
    }
    
    stat_stat++;
    
mylog(":::find node %p [%s]\n", node, node->name!=NULL?node->name:"<null>");
    /* fill the answer */
    getattr_from_node(node, st_data);

    return 0;
}

/* get target of a symlink. At this time symlinks are not implemented. */
static int callback_readlink(const char *path, char *buf, size_t size) {
    Node *node;
    int lng;
mylog("::readlink(%s)\n", path);
    node = get_node_path(path);
    if (node == NULL) {
        return(-ENOENT);
    }

    stat_stat++;

    /* not a symlink */
    if (node->symlink == NULL) {
        return(-EINVAL);
    }
    /* check length */
    lng = strlen(node->symlink);
    if ((size <= 0)||(lng <= 0)) {
        return(-EINVAL);
    }
    strncpy(buf, node->symlink, MIN(size,lng));
    if (MIN(size,lng) == size) {
      buf[size-1] = '\0';
    } else {
      buf[lng] = '\0';
    }
    return 0;
}

/* typedef int(* fuse_fill_dir_t)(void *buf, const char *name, const struct stat *stbuf, off_t off)
   returns 1 if full, 0 else
   buf: buf passed to readdir
   name: name of the dir entry
   stbuf: entry attributes (can be NULL)
   off: offset to next entry (or 0) */
/* get content of a directory */
static int callback_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    Node *node;
    int i, cur, ret;
    struct stat stmp;

    (void) fi;
mylog("::readdir(%s)\n", path);

    node = get_node_path(path);
    if (node == NULL) {
        return(-ENOENT);
    }

    /* check offset */
    if (offset-2 >= node->nb_entries) {
        /* request for an offset outbound */
mylog(":::EINVAL: '%s', offset=%d\n", path, (int)offset);
	return(0);
    }

    stat_dir++;

    /* Note: here I suppose that we *always* able to store '.' & '..'
             in the buffer. If not, this may failed/crash/whatever */

    /* 1st call? */
    if (offset == 0) {
        
	getattr_from_node(node, &stmp);
        filler(buf, ".", &stmp, 1);
	getattr_from_node(node->parent, &stmp);
	/* any entry? */
	if (node->nb_entries == 0)
	    filler(buf, "..", &stmp, 0); /* no -> no 'next' */
	else
            filler(buf, "..", &stmp, 2); /* set next */
	cur = 2;
	
	/* fill with entries until buffer is full */
	for(i=0; i<node->nb_entries; i++) {
	    getattr_from_node(node->entries[i], &stmp);
	    /* for last, prevent offset */
	    if (i == node->nb_entries-1)
	        ret = filler(buf, node->entries[i]->name, &stmp, 0);
	    else
	        ret = filler(buf, node->entries[i]->name, &stmp, ++cur);
	    /* full? quit the loop */
	    if (ret == 1)
	        break;
	}
	/* done */
	return(0);
    }
    
    /* else this is a "continue". Start from given offset */
    cur = (int)offset;
    for(i=cur-2; i<node->nb_entries; i++) {
	getattr_from_node(node->entries[i], &stmp);
	/* for last, prevent offset */
	if (i == node->nb_entries-1)
	    ret = filler(buf, node->entries[i]->name, &stmp, 0);
	else
	    ret = filler(buf, node->entries[i]->name, &stmp, ++cur);
	/* full? quit the loop */
	    if (ret == 1)
	        break;
    }
    
    /* done */
    return(0);
}

static int callback_mknod(const char *path, mode_t mode, dev_t rdev) {
  (void)path;
  (void)mode;
  (void)rdev;
  return(-EROFS);
}

static int callback_mkdir(const char *path, mode_t mode)
{
  (void)path;
  (void)mode;
  return(-EROFS);
}

static int callback_unlink(const char *path) {
  (void)path;
  return(-EROFS);
}

static int callback_rmdir(const char *path) {
  (void)path;
  return(-EROFS);
}

static int callback_symlink(const char *from, const char *to) {
  (void)from;
  (void)to;
  return(-EROFS);
}

static int callback_rename(const char *from, const char *to) {
  (void)from;
  (void)to;
  return(-EROFS);
}

static int callback_link(const char *from, const char *to) {
  (void)from;
  (void)to;
  return(-EROFS);
}

static int callback_chmod(const char *path, mode_t mode) {
  (void)path;
  (void)mode;
  return(-EROFS);
    
}

static int callback_chown(const char *path, uid_t uid, gid_t gid) {
  (void)path;
  (void)uid;
  (void)gid;
  return(-EROFS);
}

static int callback_truncate(const char *path, off_t size) {
	(void)path;
  	(void)size;
  	return(-EROFS);
}

static int callback_utime(const char *path, struct utimbuf *buf) {
    (void)path;
    (void)buf;
    return(-EROFS);
}

static int callback_open(const char *path, struct fuse_file_info *finfo) {
    Node *node;


mylog("::open(%s)\n", path);
    /* We allow opens, unless they're tring to write, sneaky
     * people. */
    int flags = finfo->flags;

    if ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & O_TRUNC) || (flags & O_APPEND)) {
        return(-EROFS);
    }

    node = get_node_path(path);
    if (node == NULL) {
        return(-ENOENT);
    }
  
    /* create the associated cache */
    if ((node->file)&&(!node->special)) {  /* only handle cache for files */
        /* do not create cache for empty files */
	if (node->size > 0) {
            if (cache_create(path, node->size) == NULL) {
		/* something goes wrong. Refuse open */
		return(-EBUSY);
	    }
	}
    }
    stat_open++;
    stat_used++;

    /* ok */
    return(0);
}

static int callback_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    char buffer[MAX_NAME], *tmp;
    int lng;
    int res;
    Node *node;
    time_t ttmp;
    unsigned int totread;
    unsigned int curoffset;
    unsigned int cursize;

    (void)finfo;

mylog("::read(%s, %p, %u, %u, -)\n", path, buf, (unsigned int)size,
      (unsigned int)offset);
    node = get_node_path(path);
    if (node == NULL) {
        return(-ENOENT);
    }
    
    stat_read++;
    
    /* special treatment for "special" files */
    if (node->special) {
mylog(":::this node is special! (type=%d)\n", node->special);
        /* only this one supported at this time */
	if (node->special == 1) {
	    /* if offset > 0, do nothing: this is a one-shor read */
	    if (offset > 0)
	        return(0);
	    /* return current time */
	    ttmp = time(NULL);
	    tmp = ctime(&ttmp);
	    if (tmp == NULL) {
	        return(-EBUSY);
	    }
	    buffer[0] = '\0';
	    strcat(buffer, tmp);
	    lng = strlen(buffer);
	    strncpy(buf, buffer, MIN(size,lng));
	    return(MIN(size,lng));
	    
	} else if (node->special == 2) { /* internal status */
	    /* if offset > 0, do nothing: this is a one-shot read */
	    if (offset > 0)
	        return(0);
    	    if (update_ok == UP_OK) {
	        sprintf(buffer, "Update ok (last update at %u)\n"
		        "%d entries in FS tree.\n", last_dl, update_nbent);
	    } else {
	        sprintf(buffer, "Error: %s\n", update_str[update_ok]);
	    }
	    lng = strlen(buffer);
	    strncpy(buf, buffer, MIN(size,lng));
	    return(MIN(size,lng));
	    
	} else if (node->special == 3) { /* internal info */
	    /* if offset > 0, do nothing: this is a one-shot read */
	    if (offset > 0)
	        return(0);
	    sprintf(buffer, "Base URL: %s\nMetadata: %s\n"
	            "Update interval: %u\n"
		    "# chunks / size: %u / %u\n", url_path, metaurl, intv_dl,
		    cache_chunks, cache_chunksize);
	    lng = strlen(buffer);
	    strncpy(buf, buffer, MIN(size,lng));
	    return(MIN(size,lng));
	    
	} else if (node->special == 4) { /* webfs data */
	    /* if offset > 0, do nothing: this is a one-shot read */
	    if (offset > 0)
	        return(0);
	    sprintf(buffer, "%s V%s\n%s", webfsName,
	            webfsVersion, webfsDescription);
	    lng = strlen(buffer);
	    strncpy(buf, buffer, MIN(size,lng));
	    return(MIN(size,lng));
	    
	} else if (node->special == 5) { /* stats data */
	    /* if offset > 0, do nothing: this is a one-shot read */
	    if (offset > 0)
	        return(0);
	    sprintf(buffer, "Access statistics:\n"
	         "Current opened files: %d\n"
		 "Total number of [fl]stat, access...: %llu\n"
		 "Total number of dir access: %llu\n"
		 "Total number of open: %llu\n"
		 "Total number of read: %llu\n"
		 "Total number of bytes read: %llu\n",
		 stat_used, stat_stat, stat_dir, stat_open, stat_read,
		 stat_data);
	    lng = strlen(buffer);
	    strncpy(buf, buffer, MIN(size,lng));
	    return(MIN(size,lng));
	    
	} else {
            return(-EOPNOTSUPP);
        }
    }
    
    /* for empty files, just do nothing */
    if (node->size == 0) {
        return(0);
    }
    
    /* loop on read until we reach the requested size */
    totread = 0;
    curoffset = offset;
    cursize = size;
    while(1) {
    
      /* call the cache system to get data from file */
      res = cache_read(node->size, path, curoffset, cursize, buf+totread);
      /* just give the result */
      mylog("::read(%s, %u, %u, %p) = %d\n", path, curoffset,
               cursize, buf+totread, res);

      if (res == 0) {
        /* let stop, no more to read */
	break;
      }
      
      totread += res;
      
      /* same size? stop */
      if (totread >= size)
        break;
      
      /* need to loop again. compute new offset/size */
      curoffset += res;
      cursize -= res;
    }
    if (totread >= 0)
      stat_data += totread;
    return(totread);
}

static int callback_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo) {
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)finfo;
    return(-EROFS);
}

static int callback_statfs(const char *path, struct statvfs *st_buf) {
    (void)path;

  mylog("::statfs(%s)\n", path);
    /* fixed values */
    st_buf->f_bsize = 4096;
    st_buf->f_frsize = 4096;
    st_buf->f_flag = ST_RDONLY | ST_NOSUID; /* to be sure */
    st_buf->f_namemax = MAX_NAME;  /* internal name representation */

    stat_stat++;

    return(0);
}

static int callback_release(const char *path, struct fuse_file_info *finfo) {
    (void) path;
    (void) finfo;
    mylog("::release(%s, -)\n", path);
    /* close the cache entry if any. just don't check if it is a file
       or if it is opened. */
    cache_destroy(path);
    
    stat_used--;
    if (stat_used < 0)
      stat_used = 0;
    
    /* call update-meta to check for new stuff */
    update_meta_if_needed();
    
    return(0);
}

static int callback_fsync(const char *path, int crap, struct fuse_file_info *finfo) {
    (void) path;
    (void) crap;
    (void) finfo;
    return(0);
}

static int callback_access(const char *path, int mode) {
    Node *node;

mylog("::access(%s)\n", path);
    node = get_node_path(path);
    if (node == NULL) {
        return(-ENOENT);
    }

    if (mode & W_OK)
        return(-EROFS);

    /* in fact we ignore access... */
    return(0);
}

/*
 * Set the value of an extended attribute
 */
static int callback_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return(-EOPNOTSUPP);
}

/*
 * Get the value of an extended attribute.
 */
static int callback_getxattr(const char *path, const char *name, char *value, size_t size) {
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    return(-EOPNOTSUPP); /* pretend it is not supported */
    /*
    int res;
    
    path=get_node_path(path);
    res = lgetxattr(path, name, value, size);
    free((char*)path);
    if(res == -1) {
        return -errno;
    }
    return res;
    */
}

/*
 * List the supported extended attributes.
 */
static int callback_listxattr(const char *path, char *list, size_t size) {
    (void)path;
    (void)list;
    (void)size;
    return(-EOPNOTSUPP); /* pretend it is not supported */
    
    /*
	int res;
	
	path=get_node_path(path);
	res = llistxattr(path, list, size);
   	free((char*)path);
    if(res == -1) {
        return -errno;
    }
    return res;
    */
}

/*
 * Remove an extended attribute.
 */
static int callback_removexattr(const char *path, const char *name) {
    (void)path;
    (void)name;
    return(-EROFS);
}

struct fuse_operations callback_oper = {
    .getattr	= callback_getattr,
    .readlink	= callback_readlink,
    .readdir	= callback_readdir,
    .mknod		= callback_mknod,
    .mkdir		= callback_mkdir,
    .symlink	= callback_symlink,
    .unlink		= callback_unlink,
    .rmdir		= callback_rmdir,
    .rename		= callback_rename,
    .link		= callback_link,
    .chmod		= callback_chmod,
    .chown		= callback_chown,
    .truncate	= callback_truncate,
    .utime		= callback_utime,
    .open		= callback_open,
    .read		= callback_read,
    .write		= callback_write,
    .statfs		= callback_statfs,
    .release	= callback_release,
    .fsync		= callback_fsync,
    .access		= callback_access,

    /* Extended attributes support for userland interaction */
    .setxattr	= callback_setxattr,
    .getxattr	= callback_getxattr,
    .listxattr	= callback_listxattr,
    .removexattr= callback_removexattr
};
enum {
    KEY_HELP,
    KEY_VERSION,
};

static void usage(const char* progname)
{
    fprintf(stdout,
"usage: %s URL [options] mountpoint\n"
"\n"
"   Mounts URL content as a local (readonly) filesystem\n"
"\n"
"general options (FUSE):\n"
"   -o opt,[opt...]     mount options\n"
"   -h  --help          print help\n"
"   -V  --version       print version\n"
"specific options:\n"
"   --url <URL>         URL to mount\n"
"   --metadata <file>   filename on webserver with metadata\n"
"   --chunks <N>        set number (max) of chunks per cache\n"
"   --chunksize <size>  set size (int byte) of chunks\n"
"   --metafile <file>   local filename for metadata (dl or generated)\n"
"   --readahead         not implemented yet\n"
"   --execfiles         force all files to be executable\n"
"\n", progname);
}

typedef struct {
  char *path;      /* URL to connect to */
  char *metadata;  /* name of metadata file in URL */
  int readahead;   /* allow read-ahead */
  int chunks;      /* number of chunks per cache */
  int chunksize;   /* size (in byte) of a chunk */
  char *metafile;  /* filename for local metadata file */
}MyOptions;

MyOptions mo = { NULL, NULL, 0, 0, 0, NULL };


#define OPTK_READAHEAD 2
#define OPTK_METADATA  3
#define OPTK_URL       4
#define OPTK_EXEC      5

static int rofs_parse_opt(void *data, const char *arg, int key,
        struct fuse_args *outargs) {
    (void) data;

    switch (key)
    {
        case FUSE_OPT_KEY_NONOPT:
	    return(1);  /* this is for fuse */
        case FUSE_OPT_KEY_OPT:
            return 1;   /* this is for fuse */
        case KEY_HELP:
            usage(outargs->argv[0]);
            exit(0);
        case KEY_VERSION:
            fprintf(stdout, "WebFS version %s\n", webfsVersion);
            exit(0);
        case OPTK_READAHEAD:
            mo.readahead = 1;
            return(0);
        case OPTK_EXEC:
            opt_exec_files = 1;
            return(0);
        default:
            fprintf(stderr, "see `%s -h' for usage (arg=%s, key=%d)\n", outargs->argv[0], arg, key);
            exit(1);
    }
    return 1;
}

static struct fuse_opt rofs_opts[] = {
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_KEY("--readahead", OPTK_READAHEAD),
    FUSE_OPT_KEY("readahead", OPTK_READAHEAD),
    FUSE_OPT_KEY("--execfiles", OPTK_EXEC),
    FUSE_OPT_KEY("execfiles", OPTK_EXEC),
    {"--metadata=%s", offsetof(MyOptions, metadata), -1},
    {"metadata=%s", offsetof(MyOptions, metadata), -1},
    {"--url=%s", offsetof(MyOptions, path), -1},
    {"url=%s", offsetof(MyOptions, path), -1},
    {"--chunks=%d", offsetof(MyOptions, chunks), -1},
    {"chunks=%d", offsetof(MyOptions, chunks), -1},
    {"--chunksise=%d", offsetof(MyOptions, chunksize), -1},
    {"chunksise=%d", offsetof(MyOptions, chunksize), -1},
    {"--metafile=%s", offsetof(MyOptions, metafile), -1},
    {"metafile=%s", offsetof(MyOptions, metafile), -1},
    FUSE_OPT_END
};

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int res, nb, fd;
    FILE *f;


    /* initialise cache system */
    if (!wget_init()) {
      fprintf(stderr, "Failed to initialize CURL library. Abort.\n");
      exit(3);
    }
    if (!cache_init()) {
      fprintf(stderr, "Failed to initialize cache system. Abort.\n");
      exit(3);
    }

    res = fuse_opt_parse(&args, &mo, rofs_opts, rofs_parse_opt);
    if (res != 0)
    {
        fprintf(stderr, "Invalid arguments\n");
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }

    /* copy args in local */
    if (mo.path == NULL) {
        fprintf(stderr, "Missing URL. See -h for help.\n");
        exit(1);
    }

    /* if mo.metadata == NULL, keep default value */
    url_path = strdup(mo.path);
    if (mo.metadata != NULL)
      url_metadata = strdup(mo.metadata);
    if ((url_path == NULL)||(url_metadata == NULL)) {
        fprintf(stderr, "Internal error while copying options. Abort.\n");
        exit(1);
    }

    if ((mo.chunks > 0)&&(mo.chunks <= CACHE_MAX_CHUNK)) {
      cache_chunks = mo.chunks;
    } else {
      if ((mo.chunks < 0)&&(mo.chunks > CACHE_MAX_CHUNK)) {
        fprintf(stderr, "Invalid number of chunks '%d' (allowed: 1-%d)\n",
	    mo.chunks, CACHE_MAX_CHUNK);
	exit(1);
      }
    }

    if (mo.chunksize >= 512) {
      cache_chunksize = mo.chunksize;
    } else {
      if (mo.chunksize > 0) {
        fprintf(stderr, "Chunk size '%d' too small (min: 512).\n", mo.chunksize);
 	exit(1);
      }
    }

    /* check: if using a updater program for metadata file,
       this one must be set with --metafile */
    if ((url_metadata[0] == '@')&&(mo.metafile == NULL)) {
      fprintf(stderr, "Problem: you set a updater program name, but you\n"
      "did not set a name for the local metafil (--metafile option), so\n"
      "you program has no way to know which file to update.\n"
      "Abort.\n");
      exit(8);
    }

    /* temp file to get the metadata for filesystem */
    /* does user gives a metafile name? */
    tpl[0] = '\0';
    if (mo.metafile != NULL) {
      strcat(tpl, mo.metafile);
    } else {
      strcat(tpl, "/tmp/webfsdesc.XXXXXX");
      fd = mkstemp(tpl);
      if (fd < 0) {
	fprintf(stderr, "Failed to create temporaty file.\n");
	exit(5);
      }
      close(fd); /* we use a FILE, not a fd */
    }
    
    /* build URL of metadata */
    metaurl[0] = '\0';
    /* special case: if url_metadata starts by @ it is the
       name of the program to use to update the code */
    if (url_metadata[0] == '@') {
      strcat(metaurl, url_metadata);
    } else {
      strcat(metaurl, wget_encode(url_path, url_metadata));
    }
    if (!load_metadata()) {
      fprintf(stderr, "Failed to download metadata file.\n");
      exit(5);
    }

    /* load metadata to create FS tree */
    f = fopen(tpl, "r");
    if (f == NULL) {
        fprintf(stderr, "Failed to open description file. Abort.\n");
	exit(9);
    }
    nb = tree_create(f);
    if (nb <= 0) {
        /* this is an error */
	fprintf(stderr, "Error while loading filesystem description. Abort.\n");
	exit(1);
    }
    printf("%d entries added in FS tree.\n", nb);
    tree_print();
    fclose(f);
    update_ok = UP_OK;
    update_nbent = nb;

    printf("Info: chunksize: %d, #chunks: %d\n", cache_chunksize, cache_chunks);

    /* only for fuse 26. else remove the final NULL */
    fuse_main(args.argc, args.argv, &callback_oper, NULL);


    /* terminate everythings */
    tree_free();
    cache_fini();
    wget_fini();
    
    /* remove temp file */
    if (url_metadata[0] != '@') /* do not remove in this case */
      unlink((const char *)tpl);
    
    return(0);
}

