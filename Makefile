##############################################################
#
# File Name : Makefile
#
# Author : Hu Lizhen
#
# Create Date : 2012-03-14
#
##############################################################

# Name of current library.
LIBNAME = rtspcli

# Libraries needed.
LDLIBS += -l$(LIBNAME)
LDLIBS += -lpthread

# project directories
ROOTDIR := ..
DEMODIR := demo
SRCDIR := src
LIBDIR := .
INCDIR := inc
TMPDIR := tmp

# Compile command.
#CROSS ?= arm-hismall-linux-
CROSS ?=
CC = $(CROSS)gcc
LD = $(CROSS)ld
AR = $(CROSS)ar
AS = $(CROSS)as
ARFLAGS = rcsv
STRIP = $(CROSS)strip
CFLAGS += -I$(INCDIR)
CFLAGS += -L$(LIBDIR)
#CFLAGS += -O2
CFLAGS += -Wall -Werror -Wno-unused
CFLAGS += -g -rdynamic
CFLAGS += -D_GNU_SOURCE
CFLAGS += -D_REENTRANT -pipe
CFLAGS += -fsigned-char
CFLAGS += -fno-strict-aliasing
RM = rm -rfv
CP = cp -rfv

# build directories
DEPDIR := $(TMPDIR)/dep
OBJDIR := $(TMPDIR)/obj

# project and build files
SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(patsubst $(SRCDIR)/%.c,$(DEPDIR)/%.d,$(SRCS))

LIB := lib$(LIBNAME).a
DEMO := $(LIBNAME)_demo

# default target : generate lib & demo
all : $(LIBDIR)/$(LIB) $(DEMODIR)/$(DEMO)

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif

$(DEMODIR)/$(DEMO) : $(DEMODIR)/*.c $(LIBDIR)/$(LIB)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)
	$(STRIP) $@

$(LIBDIR)/$(LIB) : $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

# Compile all source files, create directory if it doesn't exist.
$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@set -e; \
	if test ! -d $(OBJDIR); then \
		mkdir -p $(OBJDIR); \
	fi
	$(CC) $(CFLAGS) -o $@ -c $<

# Generate depend files, create directory if it doesn't exist.
$(DEPDIR)/%.d : $(SRCDIR)/%.c
	@set -e; \
	if test ! -d $(DEPDIR); then \
		mkdir -p $(DEPDIR); \
	fi
	$(CC) -MM $(CFLAGS) $< | sed 's,\(.*\)\.o[ :]*,$(OBJDIR)/\1.o $@: ,g' > $@

.PHONY : clean

clean :
	@$(RM) \
		$(TMPDIR) \
		$(LIBDIR)/$(LIB) \
		$(DEMODIR)/$(DEMO)
