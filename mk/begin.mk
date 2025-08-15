.SECONDEXPANSION: # Required for object-source conversion in rules
.SUFFIXES: # Remove all implicit rules

# Host detection
UNAME_SM := $(shell uname -s -m)
ifeq ($(UNAME_SM),Darwin arm64)
APPLE_SILICON := 1
endif

# Path management
MkTargetDir = @mkdir -p "$(@D)"
ThisDir = $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

# Deduce a product identifier from the Makefile path
_MP := $(firstword $(MAKEFILE_LIST))
_MF := $(notdir $(_MP))
_PF := $(patsubst Makefile.%.mk,%,$(_MF))
_MPSTEM := $(basename $(notdir $(patsubst %/,%,$(dir $(abspath $(_MP))))))
PRODUCT ?= $(if $(filter $(_MF),Makefile),$(_MPSTEM),$(_PF))

# Build directories
DISTDIR ?= build
BUILDDIR ?= $(DISTDIR)/$(PRODUCT)
OBJDIR ?= $(BUILDDIR).intermediate

# C/C++/Objective-C rules
CCFLAGS ?= -I.
CFLAGS ?= -std=c11
CXXFLAGS ?= -std=c++14
OBJCCFLAGS ?= -fobjc-arc

LINK.cc = $(CXX) $(ARCHFLAGS) $(CCFLAGS) $(CXXFLAGS) $(LDFLAGS)

$(OBJDIR)/%.s.o: %.s
	$(MkTargetDir)
	$(CC) -x assembler-with-cpp $(ARCHFLAGS) $(ASFLAGS) -c "$<" -o "$@" 

$(OBJDIR)/%.c.o: %.c
	$(MkTargetDir)
	$(CC) -MMD -MP $(ARCHFLAGS) $(CCFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(OBJDIR)/%.cpp.o: %.cpp
	$(MkTargetDir)
	$(CXX) -MMD -MP $(ARCHFLAGS) $(CCFLAGS) $(CXXFLAGS) -c "$<" -o "$@"

$(OBJDIR)/%.cc.o: %.cc
	$(MkTargetDir)
	$(CXX) -MMD -MP $(ARCHFLAGS) $(CCFLAGS) $(CXXFLAGS) -c "$<" -o "$@"

$(OBJDIR)/%.m.o: %.m
	$(MkTargetDir)
	$(CXX) -MMD -MP $(ARCHFLAGS) $(CCFLAGS) $(OBJCCFLAGS) $(OBJCFLAGS) -c "$<" -o "$@"

$(OBJDIR)/%.mm.o: %.mm
	$(MkTargetDir)
	$(CXX) -MMD -MP $(ARCHFLAGS) $(CCFLAGS) $(CXXFLAGS) $(OBJCCFLAGS) $(OBJCXXFLAGS) -c "$<" -o "$@"
	
$(OBJDIR)/%.nib: %.xib
	$(MkTargetDir)
	ibtool --compile "$@" "$<"

# Basic configuration
clean:
	rm -rf $(BUILDDIR) $(OBJDIR)
distclean:
	rm -rf $(DISTDIR)
.PHONY: all clean distclean run debug
.DEFAULT_GOAL := all

# Local rules (to keep out of the VCS tree)
-include local.mk
-include local.$(PRODUCT).mk
