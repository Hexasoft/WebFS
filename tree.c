#include "tree.h"
#include "tools.h"


int tree_debug = 0;


/* this is the root node, always created */
Node root={NULL,0,0,0,0,0,0,0,NULL,NULL,NULL,0,0,0,NULL};
/* timestamp of the tree content. to be compared with meta-data
    to decide if an update is needed */
unsigned int update=0;


/* hash table for nodes */
Node **hash_nodes=NULL;
unsigned int hash_size=0;
unsigned int hash_col=0;


/* note: this hash system is not very nice. In particular you
   can fall in infinite loop in some cases (that should not occur...) */


/* put hash */
int tree_push_hash(const char *name, Node *node) {
  unsigned int hash;
  int cnt = 0;
  
  if ((node == NULL)||(hash_nodes == NULL))
    return(0);
  hash = str_hash(name)%hash_size;
  /* if not available, search a free place */
  if (hash_nodes[hash] != NULL) {
    while(hash_nodes[hash] != NULL) {
      hash_col++;
      hash = (hash+1)%hash_size;
      cnt++;
      if (cnt >= hash_size) {
        /* eek. hash table smaller than number of entries. Kill! */
	exit(9);
      }
    }
  }
  /* insert */
  hash_nodes[hash] = node;
  
  return(0);
}

/* search in hash */
Node *tree_search_hash(const char *name) {
  unsigned int hash;
mylog("::tree_search_hash(%s)\n", name);
  if (hash_nodes == NULL)
    return(NULL);

  hash = str_hash(name+1)%hash_size;
mylog(":::tree_search_hash: hash value is %u\n", hash);
  /* loop to find the good one */
  while(hash_nodes[hash] != NULL) {
    /* prevent NULL entries */
    if (hash_nodes[hash]->fullname == NULL) {
      hash = (hash+1)%hash_size;
      continue;
    }
    if (strcmp(hash_nodes[hash]->fullname, name+1) == 0) {
      return(hash_nodes[hash]);
    }
    hash = (hash+1)%hash_size;
  }

  return(NULL);
}

int tree_init_hash(int size) {
    int i;

  /* re-alloc hash-table */
  if (hash_nodes != NULL)
    free(hash_nodes);
  
  hash_size = size + 0.9*size;
  hash_nodes = malloc(sizeof(Node*)*hash_size);

  if (hash_nodes == NULL) {
    /* hash disabled... */
    return(0);
  }
  /* init */
  for(i=0; i<hash_size; i++) {
    hash_nodes[i] = NULL;
  }
  return(1);
}


/* initialize FS tree. Do not call on an existing tree, as it
   will not cleanup the allocated memory */
void tree_init() {
  root.parent = &root;
  root.stamp = 0;
  root.name = "/";
  root.fullname = "/";
  root.file = 0;
  root.special = 0;
  if (root.entries != NULL)
    free(root.entries);
  root.entries = NULL;
  root.nb_entries = 0;
  root.m_own = 7;
  root.m_grp = root.m_other = 5;
  root.symlink = NULL;
  update = 0;
  
  hash_nodes = NULL;
  hash_size = 0;
  hash_col = 0;
}

/* recursive function for tree_free */
void r_tree_free(Node *node) {
  int i;

  /* dir: recursively treat entries, then destroy */
  if (!node->file)
    for(i=0; i<node->nb_entries; i++)
      if (node->entries[i] != NULL)
	r_tree_free(node->entries[i]);

  if (node->name != NULL)
    free(node->name);
  if (node->fullname != NULL)
    free(node->fullname);
  if (node->symlink != NULL)
    free(node->symlink);
  if (node->entries != NULL)
    free(node->entries);
  free(node);
}

/* cleanup all the tree */
void tree_free() {
  int i;
  for(i=0; i<root.nb_entries; i++)
    if (root.entries[i] != NULL)
      r_tree_free(root.entries[i]);
  if (hash_nodes != NULL)
    free(hash_nodes);
  tree_init(); /* root never destroyed */
}


/* search the node corresponding to given entry */
Node *tree_search(const char *path) {
  char *mpath;
  char *cur;
  Node *node;
  int ok, i;
mylog("::tree_search(%s)\n", path);
  /* easy: / */
  if (strcmp(path, "/") == 0) {
mylog(":::tree_search: this is sparta!\n");
    return(&root);
  }
  
  /* if available, we use hash table */
  if (hash_nodes != NULL) {
mylog(":::tree_search: using hash...\n");
    return(tree_search_hash(path));
  }
  
mylog(":::tree_search: using recursive...\n");
  /* if no hash, use old search method */
  mpath = strdup(path+1);
  if (mpath == NULL)
    return(NULL);


  /* search 1st part in /, them 2nd in node found, then... */  
  cur = strtok(mpath, "/");
  node = &root;
  while(cur != NULL) {

    ok = 0;
    for(i=0; i<node->nb_entries; i++)
      if (node->entries[i] != NULL) {
        if (strcmp(node->entries[i]->name, cur) == 0) {
	  node = node->entries[i];
	  cur = strtok(NULL, "/");
	  ok = 1;
	  break;
	}
      }

    if (!ok) {
      free(mpath);
      return(NULL);
    }
  }
  free(mpath);
  return(node);
}


/* recursive part of tree_search_inode */
Node *r_tree_search_inode(unsigned int inode, Node *node) {
  int i;
  Node *tmp;

  if (node == NULL)
    return(NULL);

  if (inode == node->inode)
    return(node);
  
  /* loop on entries */
  for(i=0; i<node->nb_entries; i++) {
    if (node->entries[i] != NULL) {
      tmp = r_tree_search_inode(inode, node->entries[i]);
      if (tmp != NULL)
        return(tmp);
    }
  }
  return(NULL);
}


/* search a node by inode
   returns pointer to the Node or NULL if not found */
Node *tree_search_inode(unsigned int inode) {

  return(r_tree_search_inode(inode, &root));

}

/* just print the full name of this particular node */
void tree_print_name(Node *node) {
  if (node->parent == node) {
    printf("%s", node->name==NULL?"":node->name);
  } else {
    tree_print_name(node->parent);
    printf("%s%s", node->parent==node->parent->parent?"":"/", node->name);
  }
}

/* just print data in a particular node */
void tree_print_entry(Node *node) {
  printf("%p: [", node);
  tree_print_name(node);
  printf("]{%s}", node->fullname==NULL?"<null>":node->fullname);
  if (node->symlink != NULL) {
    printf("->%s", node->symlink);
  }
  printf(" %s {%d} (%u %u %u)\n", node->file?"f":"d", node->special,
          node->size, node->stamp, node->links);
}

/* recursive part of tree_print */
void r_tree_print(Node *node) {
  int i;
  
  tree_print_entry(node);
  
  if (node->file) {
    return;
  }
  
  for(i=0; i<node->nb_entries; i++) {
    if (node->entries[i] != NULL)
      r_tree_print(node->entries[i]);
  }
}


/* debug: print tree */
void tree_print() {
  printf("Tree (update=%u, collisions in hash: %u):\n", update, hash_col);
  r_tree_print(&root);
}


/* returns the 'dirname' of path. path should not ends with a /.
   use a buffer. Do not free it, do not use in thread context */
char *tree_dirname(char *path) {
  static char buffer[MAX_NAME];
  int i;

  if (path[0] != '/') {
    buffer[0] = '/';
    buffer[1] = '\0';
  } else {
    buffer[0] = '\0';
  }
  strcat(buffer, path);

  for(i=strlen(buffer)-1; i>0; i--) {
    if (buffer[i] == '/')
      break;
  }
  /* no / found? we are inside / */
  if (i == 0) {
    buffer[0] = '/';
    buffer[1] = '\0';
    return(buffer);
  }
  /* get the path */
  buffer[i] = '\0';
  return(buffer);
}

/* add a node. In fact add a slot in 'entries' */
int tree_add_node(Node *node) {
  if (node == NULL)
    return(-1);

  if (node->nb_entries == 0) {
    node->entries = malloc(sizeof(Node *));
    if (node->entries == NULL)
      return(-1);
    node->nb_entries = 1;
    return(0);
  }

  /* increase */
  node->entries = realloc(node->entries, sizeof(Node *)*(node->nb_entries+1));
  if (node->entries == NULL)
    return(-1);
  node->nb_entries++;
  /* cleanup this entry */
  node->entries[node->nb_entries-1] = NULL;
  return(node->nb_entries-1);
}

void tree_set_mode(Node *node, char *mode) {
  char *tmp;
  
  tmp = mode + (strlen(mode)-3); /* just keep last 3 digits */
  node->m_own   = tmp[0]-'0';
  node->m_grp   = tmp[1]-'0'; /* should maybe check that values are valid... */
  node->m_other = tmp[2]-'0';
}

/* this function parse tree, and validate for dirs that the  number
   of links is valid (and correct it if needed) */
int r_tree_update_links(Node *node) {
  int i, nb = 0;
  int sum = 0;  /* number of changes */
  
  for(i=0; i<node->nb_entries; i++) {
    if (node->entries[i] != NULL) {
      if (!node->entries[i]->file) {
        /* this is a dir. inc nb for local update, and treat dir */
	nb++;
	sum += r_tree_update_links(node->entries[i]);
      }
    }
  }
  /* compare value with computed one */
  if (node->links != nb + 2) {
    sum++;
    /* change the value */
    node->links = nb+2;
  }
  return(sum);
}

int tree_update_links() {
  return(r_tree_update_links(&root));
}


/* create tree from FS description (tree must be cleaned)
   returns the number of item created (or <= 0 on error) */
int tree_create(FILE *f) {
  char name[MAX_NAME], target[MAX_NAME], mode[8];
  int file, ret, nb, nbt;
  unsigned int stamp, size, links, inode;
  char *dirname, *cret;
  Node *node;
  int fp, special;
  int line = 0;

  line = 1;
  if(fscanf(f, "%u", &update) != 1) {
    fprintf(stderr, "Bad format (line %d)!\n", line);
    return(0);
  }
  line++;
  if(fscanf(f, "%d", &nbt) != 1) {
    fprintf(stderr, "Bad format (line %d)!\n", line);
    return(0);
  }

  /* initialise hash table */
  tree_init_hash(nbt);

  /* data for / */
  ret = fscanf(f, "%d%u%u%u%u%s", &file, &size, &inode, &stamp, &links, mode);
  line++;
  if (ret != 6) {
    fprintf(stderr, "Bad format (line %d)!\n", line);
    return(0);
  }
  cret = fgets(name, MAX_NAME, f); /* remove \n */
  cret = fgets(name, MAX_NAME, f);
  line++;
  if (cret == NULL) {
    fprintf(stderr, "Bad format (line %d)!\n", line);
    return(0);
  }
  if (name[strlen(name)-1] == '\n')
    name[strlen(name)-1] = '\0';
printf("# read: %d %u %u %u %u %s %s\n", file, inode, size, stamp, links, mode, name);

  root.parent = &root;
  root.m_own = 7;
  root.m_grp = root.m_other = 5;  /* ignore mode for / */
  root.inode = inode;
  root.file = 0;  /* / always a dir */
  root.special = 0;  /* never occur for / */
  root.size = size;
  root.links = links;
  root.stamp = stamp;
  root.symlink = NULL;  /* / never a symlink */
  root.fullname = "/";
  nb = 1;  /* number of created entries */

  tree_push_hash("/", &root);

  /* now treat all entries */
  while(1) {
    ret = fscanf(f, "%d%u%u%u%u%s", &file, &size, &inode, &stamp, &links, mode);
    line++;
    if (feof(f)||(ret != 6))
      break;
    cret = fgets(name, MAX_NAME, f); /* remove \n */
    cret = fgets(name, MAX_NAME, f);
    line++;
    if (cret == NULL) {
      fprintf(stderr, "Bad format (line %d)!\n", line);
      return(0);
    }
    if (name[strlen(name)-1] == '\n')
      name[strlen(name)-1] = '\0';
    //printf("# read: %d %u %u %u %u %s %s\n", file, inode, size, stamp, links, mode, name);
    if (file >= 100) {
      /* it is a file */
      special = file-100; /* type of special file */
      file = 1; /* force file */
    } else {
      special = 0;
    }
    
    /* if symlink, read target */
    if (file == 2) {
      cret = fgets(target, MAX_NAME, f);
      line++;
      if (cret == NULL) {
        fprintf(stderr, "Bad format (line %d)!\n", line);
        return(0);
      }
      if (target[strlen(name)-1] == '\n')
        target[strlen(name)-1] = '\0';
    } else {
      target[0] = '\0';
    }

    /* search the dirname */
    dirname = tree_dirname(name);
    /* search corresponding node */
    node = tree_search(dirname);
    if (node == NULL) {
      fprintf(stderr, "Entry '%s': can't find dirname node (for '%s').\n",
                       name, dirname);
      continue;
    }
    /* check that it is a directory */
    if (node->file) {
      fprintf(stderr, "Entry '%s': dirname node '%s' is a file!\n",
                       name, dirname);
      continue;
    }
    //printf("# -> dirname = '%s' (node=%p)\n", dirname, node);
    
    /* create a new node */
    fp = tree_add_node(node);
    if (fp < 0) {
      fprintf(stderr, "Failed to increase entries list size. Node probably lost.\n");
      fprintf(stderr, "We're in big trouble! Dying badly...\n");
      exit(66);
    }
    
    /* allocate it */
    node->entries[fp] = malloc(sizeof(Node));
    if (node->entries[fp] == NULL) {
      fprintf(stderr, "Failed to allocate a node. Ignoring '%s'.\n", name);
      continue;
      /* should reduce the table size, here... */
    }
    
    //printf("# adding node (%p) add pos %d in entry %p\n", node->entries[fp], fp, node);
    
    /* fill it */
    node->entries[fp]->parent = node;
    /* if symlink, use target length */
    if (file == 2) {
      node->entries[fp]->size = strlen(target);
    } else {
      node->entries[fp]->size = size;
    }
    node->entries[fp]->stamp = stamp;
    node->entries[fp]->links = links;
    node->entries[fp]->inode = inode;
    node->entries[fp]->file = file==2?1:file; /* symlinks are files */
    node->entries[fp]->special = special;
    tree_set_mode(node->entries[fp], mode);
    if (strcmp(dirname, "/") == 0)
      node->entries[fp]->name = strdup(name);
    else
      node->entries[fp]->name = strdup(name+strlen(dirname));
    node->entries[fp]->fullname = strdup(name);
    node->entries[fp]->nb_entries = 0;
    node->entries[fp]->entries = NULL;
    if (target[0] == '\0') {
      node->entries[fp]->symlink = NULL;
    } else {
      node->entries[fp]->symlink = strdup(target);
    }

    tree_push_hash(name, node->entries[fp]);

    /* lack checking for failed strdup() */

    nb++;
  }

  /* update the number of links for dirs */
  tree_update_links();

  return(nb);
}
