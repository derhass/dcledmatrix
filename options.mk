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
# compile time configuration options

# set RELEASE to 1 for a release build
RELEASE=0

# set to 1 to build the OBS plugin
OBS=1

# include the build system configuration itself
include ${TOP}/config.mk

