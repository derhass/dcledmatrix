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
 
# use the build rules from the main makefiles
include ${TOP}/dclm.mk

.PHONY: distclean
distclean: clean
	-@rmdir $(BUILDPATH)/base $(BUILDPATH)/common
	-@rmdir $(BUILDPATH) $(BINPATH) $(LIBPATH)
	-@rm -f dclm.tgz

dclm.tgz:
	tar cfz dclm.tgz $(SUBDIRS) $(EXTRAS)

.PHONY: dist
dist: dclm.tgz

