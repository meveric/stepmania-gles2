#!/bin/bash
set -e

if test "$1" = ""; then
	echo Expected argument
	exit 1
fi

if ! test -e $1; then
	echo "File $1 doesn't exist"
	exit 1
fi

PRODUCT=`cat $1 | grep -w '^.define PRODUCT_FAMILY_BARE' | sed -re 's/.*_BARE +(.*)/\1/'`
VER=`cat $1 | grep -w '^.define PRODUCT_VER_BARE' | sed -re 's/.*_BARE +(.*)/\1/'`

# "3.9 alpha 1" -> "3.9 alpha1"
VER=`echo "$VER" | sed -e 's/alpha /alpha/'`
# "3.9 beta 1" -> "3.9 beta1"
VER=`echo "$VER" | sed -e 's/beta /beta/'`
# "3.9 alpha1" -> "3.9-alpha1"
VER=`echo "$VER" | sed -e 's/ /-/g'`

PRODUCTVER="$PRODUCT-$VER"
