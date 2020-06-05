# Makefile for unix systems
# this requires GNU make
# 

# top directory
TOP = ..

# get the options
include ${TOP}/options.mk

INCLUDEFLAGS = -I${TOP}/base -I${TOP}/common
LINK = -lrt -pthread -lhidapi-libusb

NAME=dclmtest
MODULE=test

# build a binary	
BINARY=1

# source files
SRCFILES=test \
	 ${TOP}/base/dclm \
	 ${TOP}/common/dclmd_comm \
	 ${TOP}/common/dclm_font \
	 ${TOP}/common/dclm_image

# use the build rules from the main makefiles
include ${TOP}/dclm.mk

