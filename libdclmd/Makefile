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
SRCFILES=${TOP}/common/dclm_font \
	 ${TOP}/common/dclm_image \
         ${TOP}/common/dclmd_comm

# use the build rules from the main makefiles
include ${TOP}/dclm.mk

