# Copyright 2005-2012 Intel Corporation.  All Rights Reserved.
#
# The source code contained or described herein and all documents related
# to the source code ("Material") are owned by Intel Corporation or its
# suppliers or licensors.  Title to the Material remains with Intel
# Corporation or its suppliers and licensors.  The Material is protected
# by worldwide copyright laws and treaty provisions.  No part of the
# Material may be used, copied, reproduced, modified, published, uploaded,
# posted, transmitted, distributed, or disclosed in any way without
# Intel's prior express written permission.
#
# No license under any patent, copyright, trade secret or other
# intellectual property right is granted to or conferred upon you by
# disclosure or delivery of the Materials, either expressly, by
# implication, inducement, estoppel or otherwise.  Any license under such
# intellectual property rights must be express and approved by Intel in
# writing.

####### Detections and Commands ###############################################

ifeq (icc,$(compiler))
  export COMPILER_VERSION := ICC: $(shell icc -V </dev/null 2>&1 | grep 'Version')
  ifneq (,$(findstring IA-32, $(COMPILER_VERSION)))
    export arch:=ia32
  endif
  ifneq (,$(findstring Intel(R) 64, $(COMPILER_VERSION)))
    export arch:=intel64
  endif
  ifeq (,$(arch))
    $(warning "Unknown Intel compiler")
  endif
endif

ifndef arch
 ifeq ($(shell /usr/sbin/sysctl -n hw.machine),Power Macintosh)
   ifeq ($(shell /usr/sbin/sysctl -n hw.optional.64bitops),1)
     export arch:=ppc64
   else
     export arch:=ppc32
   endif
 else
   ifeq ($(shell /usr/sbin/sysctl -n hw.optional.x86_64 2>/dev/null),1)
     export arch:=intel64
   else
     export arch:=ia32
   endif
 endif
endif

ifndef runtime
 gcc_version = $(shell gcc -dumpversion)
 os_version:=$(shell /usr/bin/sw_vers -productVersion)
 export runtime:=cc$(gcc_version)_os$(os_version)
endif

native_compiler := gcc
export compiler ?= gcc
debugger ?= gdb

CMD=$(SHELL) -c
CWD=$(shell pwd)
RM?=rm -f
RD?=rmdir
MD?=mkdir -p
NUL= /dev/null
SLASH=/
MAKE_VERSIONS=sh $(tbb_root)/build/version_info_macos.sh $(CPLUS) $(CPLUS_FLAGS) $(INCLUDES) >version_string.ver
MAKE_TBBVARS=sh $(tbb_root)/build/generate_tbbvars.sh

####### Build settings ########################################################

OBJ=o
DLL=dylib
LIBEXT=dylib

def_prefix = $(if $(findstring 64,$(arch)),mac64,mac32)

TBB.LST = $(tbb_root)/src/tbb/$(def_prefix)-tbb-export.lst
TBB.DEF = $(TBB.LST:.lst=.def)
TBB.DLL = libtbb$(CPF_SUFFIX)$(DEBUG_SUFFIX).$(DLL)
TBB.LIB = $(TBB.DLL)
LINK_TBB.LIB = $(TBB.LIB)

MALLOC.DEF = $(MALLOC_ROOT)/$(def_prefix)-tbbmalloc-export.def
MALLOC.DLL = libtbbmalloc$(DEBUG_SUFFIX).$(DLL)
MALLOC.LIB = $(MALLOC.DLL)

TBB_NOSTRICT=1

TEST_LAUNCHER=sh $(tbb_root)/build/test_launcher.sh
