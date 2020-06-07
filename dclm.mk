# Makefile for unix systems:
# build rules 

ifeq ($(LIBRARY),1)
CFLAGS += $(CFLAGS_SHARED)
DEPEND=1
endif

ifeq ($(OBSPLUGIN),1)
CFLAGS += $(CFLAGS_SHARED)
DEPEND=1
endif

ifeq ($(BINARY),1)
CFLAGS += $(CFLAGS_BINARY)
DEPEND=1
endif

ifeq ($(RELEASE),1)
CFLAGS += $(CFLAGS_RELEASE)
else
CFLAGS += $(CFLAGS_DEBUG)
endif

.PHONY: default
default: all

ifeq ($(LIBRARY),1)
BUILDDIR=${BUILDPATH}/lib/${MODULE}
else
ifeq ($(OBSPLUGIN),1)
BUILDDIR=${BUILDPATH}/lib/obs-plugins/${MODULE}
else
BUILDDIR=${BUILDPATH}/${MODULE}
endif
endif
BUILDOBJECTS=$(addprefix $(BUILDDIR)/,$(SRCFILES))

CPPFLAGS += $(INCLUDEFLAGS)

# create the build directory. any intermediate files will be stored there
${BUILDDIR}:
	@echo creating internal build dir: ${BUILDDIR}
	@$(MKDIR) -p ${BUILDDIR} ${BUILDDIR}/../base ${BUILDDIR}/../common

# automatic dependency generation
# create $(DEPDIR) (and an empty file dir)
# create a .d file for every .c source file which contains
# 		   all dependencies for that file
# create file dependencies which conatians all dependencies and is
# included into this makefile
.PHONY: depend
depend:	$(BUILDDIR)/dependencies
DEPFILES = $(addsuffix .d,$(BUILDOBJECTS))
$(BUILDDIR)/dependencies: $(BUILDDIR) $(DEPFILES)
	@$(ECHO) "Rebuilding dependencies for $(MODULE)"
	@$(CAT) $(DEPFILES) > $(BUILDDIR)/dependencies
$(BUILDDIR)/%.d: %.c $(BUILDDIR)
	@set -e; $(CC) -M $(CPPFLAGS) $<	\
	| $(SED) 's,\($*\)\.o[ :]*,$(BUILDDIR)/\1.o $@ : ,g' \
	> $@; [ -s $@ ] || rm -f $@

ifeq ($(DEPEND),1)
-include $(BUILDDIR)/dependencies
ALLDEPS += $(BUILDDIR)/dependencies
BUILDFILES += $(DEPFILES) $(BUILDDIR)/dependencies 
endif


# C FILE COMPILATION
OBJECTS = $(addsuffix .o,$(BUILDOBJECTS))
BUILDFILES += $(OBJECTS)

.PHONY: compile
compile: $(OBJECTS)

$(BUILDDIR)/%.o: %.c 
	 $(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
	
# LIBRARIES 
ifeq ($(LIBRARY),1)
${LIBPATH}:
	@echo creating library dir: ${LIBPATH}
	@mkdir -p ${LIBPATH}

$(LIBPATH)/$(NAME): $(OBJECTS) $(LIBPATH)
	@echo creating library $(LIBPATH)/$(NAME)
	$(CC) $(CFLAGS) $(LINK) $(OBJECTS) -shared -Wl,-soname,lib$(APPNAME).so.$(APPVERSION_MAJOR) -o$(LIBPATH)/$(NAME)

$(LIBPATH)/lib$(APPNAME).so: $(LIBPATH)/lib$(APPNAME).so.$(APPVERSION_MAJOR)
	@ln -s lib$(APPNAME).so.$(APPVERSION_MAJOR) $(LIBPATH)/lib$(APPNAME).so

$(LIBPATH)/lib$(APPNAME).so.$(APPVERSION_MAJOR): $(LIBPATH)/$(NAME)
	@ln -s $(NAME) $(LIBPATH)/lib$(APPNAME).so.$(APPVERSION_MAJOR)

ALLDEPS += $(LIBPATH)/$(NAME)  $(LIBPATH)/lib$(APPNAME).so
BUILDFILES += $(LIBPATH)/$(NAME) $(LIBPATH)/lib$(APPNAME).so $(LIBPATH)/lib$(APPNAME).so.$(APPVERSION_MAJOR)
endif

# OBS PLUGINS
ifeq ($(OBSPLUGIN),1)
${OBSPLUGINPATH}:
	@echo creating obs plugin dir: ${obspluginpath}
	@mkdir -p ${OBSPLUGINPATH}

$(OBSPLUGINPATH)/$(NAME): $(OBJECTS) $(OBSPLUGINPATH)
	@echo creating OBS plugin: $(OBSPLUGINPATH)/$(NAME)
	$(CC) $(CFLAGS) $(LINK) $(OBJECTS) -shared -o$(OBSPLUGINPATH)/$(NAME)

ALLDEPS += $(OBSPLUGINPATH)/$(NAME)
BUILDFILES += $(OBSPLUGINPATH)/$(NAME)
endif
 
 
# BINARIES 
ifeq ($(BINARY),1)
${BINPATH}:
	@echo creating biary dir: ${BINPATH}
	@mkdir -p ${BINPATH}

$(BINPATH)/$(NAME): $(OBJECTS) $(BINPATH)
	@echo creating binary $(BINPATH)/$(NAME)
	$(CC) $(CFLAGS) $(OBJECTS) $(LINK) -o$(BINPATH)/$(NAME)

ALLDEPS += $(BINPATH)/$(NAME)
BUILDFILES += $(BINPATH)/$(NAME)
endif

# RECURSE INTO SUBDIRS
ifeq ($(SUBSYSTEM),1)
.PHONY: $(SUBDIRS)	
$(SUBDIRS):
	@$(MAKE) -C $(basename $@) all

ALLDEPS += $(SUBDIRS)

.PHONY: clean
clean: 
	@for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done

else
.PHONY: clean
clean: $(SUBDIRS)
	@$(ECHO) cleaning up $(MODULE)
	@-$(RM) -f $(BUILDFILES)
	@-$(RMDIR) $(BUILDDIR)

endif

# META RULES

.PHONY: prereq
prereq: 
	@for i in $(PREREQUISITES); do $(MAKE) -C ${TOP}/$$i all; done

.PHONY: all
all: prereq $(ALLDEPS)

