#
# dclmd: the daemon
# 
# Makefile for unix systems
# this requires GNU make
# 

# top directory
TOP = ..

# get the options
include ${TOP}/options.mk

INCLUDEFLAGS = -I${TOP}/base -I${TOP}/common
LINK = -lrt -pthread

NAME=dclmclient
MODULE=dclmclient

# build a binary	
BINARY=1

# source files
SRCFILES=dclmclient \
	 ${TOP}/common/dclmd_comm

# use the build rules from the main makefiles
include ${TOP}/dclm.mk

