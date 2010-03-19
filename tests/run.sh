#!/bin/bash

dir="`dirname $0`"
grep SUBDIRS "$dir/tests.pro" | awk '{print $NF}' | while read i; do 
    pushd $dir/$i
    qmake
    make
    echo $PWD
    ./$i
    popd
done
