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
TOP = .

# get the options
include ${TOP}/options.mk

SUBSYSTEM=1
SUBDIRS = dclmd libdclmd dclmclient test
EXTRAS = base common Makefile config.mk options.mk dclm.mk .gitignore 

ifeq ($(OBS),1)
SUBDIRS += obs
else
EXTRAS += obs
endif
 
# use the build rules from the main makefiles
include ${TOP}/dclm.mk

.PHONY: distclean
distclean: clean
	-@rm -rf $(BUILDPATH) dep
	-@rmdir $(BUILDPATH) $(BINPATH) $(OBSPLUGINPATH) $(LIBPATH)
	-@rm -f dclm.tgz

dclm.tgz:
	tar cfz dclm.tgz $(SUBDIRS) $(EXTRAS)

.PHONY: dist
dist: dclm.tgz

