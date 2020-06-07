# Makefile for unix systems
# this requires GNU make
# 

# top directory
TOP = ..

# get the options
include ${TOP}/options.mk

INCLUDEFLAGS = -I${TOP}/base -I${TOP}/common $(shell pkg-config --cflags libobs)/obs
LINK = $(shell pkg-config --static --libs libobs) -lobs -lrt -pthread
CFLAGS += -Wno-declaration-after-statement

APPNAME=mh-dcledmatrix
NAME=$(APPNAME).so
MODULE=$(APPNAME)

# build an OBS plugin
OBSPLUGIN=1

# source and header files
SRCFILES=dclm-plugin \
	 ${TOP}/common/dclmd_comm

# use the build rules from the main makefiles
include ${TOP}/dclm.mk

