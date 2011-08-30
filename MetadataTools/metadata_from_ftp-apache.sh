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
  typeset NN=""
  typeset DP=""
  typeset YEAR=""
  typeset MON=""
  typeset DAY=""
  typeset HOUR=""
  typeset UDTATE=""
  typeset SIZE=""

  MYLOG=`mktemp`
  LANG=C wget -q -O "$MYLOG" "$1/"
  if [ ! "$?" = 0 ]
  then
    echo "Failed to get URL '$1'. Abort." >&2
    cleanup
    rm -f "$MYLOG"
    exit 2
  fi

  # parse entries
  grep -w -e "Directory" -e "File" "$MYLOG" | while read N
  do
    # get name inside
    NN=`echo "$N" | cut -d">" -f2 | cut -d"<" -f1`
    # get file or directory
    TST=`echo "$N" | grep -w "File"`
    if [ "$TST" = "" ]
    then
      DIR=1
    else
      DIR=0
    fi
    # get date part
    if [ "$DIR" = 1 ]
    then
      DP=`echo "$N" | sed -e "s/Directory.*$//" | sed -e "s/^[ ]*//" | sed -e "s/[ ]*$//"`
    else
      DP=`echo "$N" | sed -e "s/File.*$//" | sed -e "s/^[ ]*//" | sed -e "s/[ ]*$//"`
    fi
    # get parts of the date
    YEAR=`echo "$DP" | cut -d" " -f1`
    MON=`echo "$DP" | cut -d" " -f2`
    DAY=`echo "$DP" | cut -d" " -f3`
    if [ `echo "$DP" | wc -w` = 4 ]
    then
      HOUR=`echo "$DP" | cut -d" " -f4`
    else
      HOUR="00:00:00"
    fi
    # get unix date
    UDATE=`LANG=C date -d "$HOUR $MON $DAY $YEAR" +"%s"`
    
    # get size
    if [ "$DIR" = 1 ]
    then
      SIZE=4096
    else
      SIZE=`echo "$N" | cut -d"(" -f2 | cut -d" " -f1`
    fi
    
    # remove final / if needed
    if [ "$DIR" = 1 ]
    then
      NB=`printf "$NN" | wc -c`
      NNB=$(($NB - 1))
      NAME=`echo "$NN" | cut -c 1-$NNB`
    else
      NAME="$NN"
    fi
      
    # add it to file list
    PNAME=`echo "$2/$NAME" | sed -e "s,//,/,g"`
    echo "$DIR $UDATE $SIZE $PNAME" >>"$FILES"

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


cat "$FILES"
cleanup
exit 0

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
  echo treat $FILE

done <"$FILES"


cleanup
