#!/bin/sh

autoscan 
cp configure.scan configure.in
aclocal
autoconf
autoheader
automake --add-missing
./configure CXXFLAGS= CFLAGS=
make
