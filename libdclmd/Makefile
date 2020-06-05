# Makefile for unix systems
# this requires GNU make
# 

# top directory
TOP = ..

# get the options
include ${TOP}/options.mk

INCLUDEFLAGS = -I${TOP}/base -I${TOP}/common
LINK = -lrt -pthread

APPVERSION_MAJOR=1
APPVERSION_MINOR=0
APPVERSION_PATCHLEVEL=0
APPNAME=dclmd
NAME=lib$(APPNAME).so.$(APPVERSION_MAJOR).$(APPVERSION_MINOR).$(APPVERSION_PATCHLEVEL)
MODULE=lib$(APPNAME)

# build a library	
LIBRARY=1

# source and header files
SRCFILES=${TOP}/common/dclmd_comm

# use the build rules from the main makefiles
include ${TOP}/dclm.mk

