#
# Copyright (C) 2011 - 2020 by derhass <derhass@arcor.de>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
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

