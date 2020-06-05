# Makefile for unix systems:
# compiler options

# C Compiler and flags
CC = gcc
CFLAGS = -Wall -pedantic -Wdeclaration-after-statement -Wmissing-prototypes -Wsign-compare

# special flags for normal binarys
CFLAGS_BINARY =

# special flags for shared libs
CFLAGS_SHARED = -fPIC -Bsymbolic

# flags for RELEASE builds 
CFLAGS_DEBUG = -g -Werror

# flags for RELEASE builds
CFLAGS_RELEASE = -ffast-math -s -O5 -DNDEBUG

# directories
BINPATH=${TOP}/bin
LIBPATH=${TOP}/lib
BUILDPATH=${TOP}/built

# unix tools to use
ECHO=echo
CAT=cat
LN=ln
RM=rm
RMDIR=rmdir
GREP=grep
SED=sed
MKDIR=mkdir

