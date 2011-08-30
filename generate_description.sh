#!/bin/sh

cd Test/

# on genere la date, pour MaJ
date +%s

# le nombre d'entrees
find . | wc -l

# parcours des entrees
find . | while read E
do
  N=`echo "$E" | sed -e "s,^./,,"`
  if [ -d "$E" ]
  then
    FILE=0
  else
    FILE=1
  fi
  
  TMP=`stat -c "%Y %s %i %h %a" "$E"`
  
  MODIF=`echo "$TMP" | cut -d" " -f1`
  SIZE=`echo "$TMP" | cut -d" " -f2`
  INODE=`echo "$TMP" | cut -d" " -f3`
  LINK=`echo "$TMP" | cut -d" " -f4`
  MODE=`echo "$TMP" | cut -d" " -f5`

  # cas d'un lien symbolique
  if [ -L "$E" ]
  then
    # this is a symlink, check for target name
    TARGET=`stat -c "%N" "$E" | cut -d">" -f2- | cut -c3- | sed -e "s/.$//"`
    TSIZE=`printf "$TARGET" | wc -c`
  else
    TARGET=""
    TSIZE=0
  fi

  if [ "$TARGET" = "" ]
  then
    echo "$FILE $SIZE $INODE $MODIF $LINK $MODE"
    echo "$N"
  else
    # lien symbolique
    echo "2 $TSIZE $INODE $MODIF $LINK $MODE"
    # format d'un symlink: nom fichier, suivi de nom de cible
    echo "$N"
    echo "$TARGET"
  fi

done

# statique : on ajoute les fichiers speciaux
cat <<_EOF_
101 1024 11502 1311861394 1 644
whattimeisit
102 1024 11002 1311861394 1 644
.status
103 1024 11003 1311861394 1 644
.info
104 1024 11004 1311861394 1 644
.webfs
105 1024 11005 1311861394 1 644
.stats
_EOF_


# pour faire un lien symbolique, il faut faire :
#
# 2 20 11500 1311947314 1 777  <- type 2, taille de la cible (lng nom)
#                              <- mode 777 (pas de ligne vide, hein)
# d1/liensymbolique.jpg        <- nom du fichier
# moi_mais_pas_moi.jpg         <- cible du lien
#
#
#
