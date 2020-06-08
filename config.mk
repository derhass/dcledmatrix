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
CFLAGS_RELEASE = -flto -ffast-math -s -O5 -DNDEBUG

# directories
BINPATH=${TOP}/bin
LIBPATH=${TOP}/lib
OBSPLUGINPATH=${TOP}/lib/obs-plugins
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

