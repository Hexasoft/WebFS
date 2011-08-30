#ifndef __tree_h_
#define __tree_h_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* max possible length for a entry name */
#define MAX_NAME 1024


/* not used anymore */
extern int tree_debug;

/* timestamp of the tree content. to be compared with meta-data
    to decide if an update is needed */
unsigned int update;

/* internal structure of a node (an entry in the filesystem) */
typedef struct _Node {
  /* parent node. / is its own parent */
  struct _Node *parent;
  /* timestamp (unix time). timestamp is for [cma]time */
  unsigned int stamp;
  /* file/dir size (byte) */
  unsigned int size;
  /* number of links (not really useful) */
  unsigned int links;
  /* inode number */
  unsigned int inode;
  /* 3 parts for entry mode. no 's' or 't' mode handled */
  char m_own, m_grp, m_other;
  /* name of entry */
  char *name;
  char *fullname;  /* full name with path */
  /* target of symlink or NULL for regular file */
  char *symlink;
  /* true if FILE, false if DIR. symlink and special are files */
  int file;
  /* if !=0, special file. content treated by program. */
  int special;
  /* number of elements in 'entries' */
  int nb_entries;
  /* table of child nodes */
  struct _Node **entries;
}Node;



/* initialise FS tree. should be called only at start */
extern void tree_init();

/* cleanup all the tree */
extern void tree_free();

/* search an entry in tree */
extern Node *tree_search(const char *path);

/* search by inode */
extern Node *tree_search_inode(unsigned int inode);

/* mostly debug: print tree content */
extern void tree_print();

/* create tree from FS description (tree must be cleaned) */
extern int tree_create(FILE *f);

/* TODO: tree_recreate() -> check if content changed are re-create
         tree if yes. */

#endif /* __tree_h_ */
