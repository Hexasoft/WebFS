#!/bin/bash

# this script builds a WebFS metadata file from a HTTP connection
#  to a apache-http site


# usage: $1 base URL

if [ "$1" = "" -o "$2" = "" -o "$1" = "-h" -o "$1" = "--help" ]
then
  echo "Usage: $0 <site> <base path>"
  exit 1
fi

if [ "$2" = "/" ]
then
  URL="$1"
else
  URL="$1$2"
fi
FILES=`mktemp`
OUTPUT=`mktemp`
TMP=`mktemp`

cleanup() {
  rm -f "$FILES" "$OUTPUT" "$TMP"
}


# this function treat an URL. In case of error it just exit.
# for each entry:
# if a directory, it adds it in $FILES and go inside (recurs)
# if a file, it only adds it in $FILES
# then it treats next entry
treat_url() {
  if [ "$1" = "" -o "$2" = "" ]
  then
    echo "Missing URL!" >&2
    cleanup
    exit 3
  fi
  
  echo "Treating '$1'..."
  
  typeset MYLOG=""
  typeset NB=""
  typeset LAST=""
  typeset NNB=""
  typeset N=""
  typeset DIR=""
  typeset NAME=""
  typeset PNAME=""

  MYLOG=`mktemp`
  wget -q -O "$MYLOG" "$1"
  if [ ! "$?" = 0 ]
  then
    echo "Failed to get URL '$URL'. Abort." >&2
    cleanup
    rm -f "$MYLOG"
    exit 2
  fi

  # parse entries
  grep "^<a href=" "$MYLOG" | cut -d"\"" -f2 | while read N
  do
    # check for final /
    NB=`printf "$N" | wc -c`
    LAST=`echo "$N" | cut -c $NB`
    
    if [ "$LAST" = "/" ]
    then
      DIR=1
      NNB=$(($NB - 1))
      NAME=`echo "$N" | cut -c 1-$NNB`
    else
      DIR=0
      NAME="$N"
    fi
    # add it to file list
    PNAME=`echo "$2/$NAME" | sed -e "s,//,/,g"`
    echo "$DIR $PNAME" >>"$FILES"

    # recurs on it
    if [ "$DIR" = 1 ]
    then
      treat_url "$1/$NAME" "$PNAME"
    fi
  done
  rm -f "$MYLOG"
}

# create files list
treat_url "$URL" "/"



###
### ALARM! at least the webserver www.kernel.org does not provide
###        'Content-length' field when replying to a 'HEAD' request
###        on a (existing) file. So we will not be able to use it...
### 

# now we have the full files/dirs list, let's extract metadata
# for each of them
while read DIR FILE
do
  # check for metadata for file $2/$FILE on $1

done <"$FILES"


cleanup
