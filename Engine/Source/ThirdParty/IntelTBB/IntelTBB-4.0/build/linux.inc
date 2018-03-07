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
  ifneq (,$(findstring IA-64, $(COMPILER_VERSION)))
    export arch:=ia64
  endif
  ifeq (,$(arch))
    $(warning "Unknown Intel compiler")
  endif
endif

ifndef arch
        uname_m:=$(shell uname -m)
        ifeq ($(uname_m),i686)
                export arch:=ia32
        endif
        ifeq ($(uname_m),ia64)
                export arch:=ia64
        endif
        ifeq ($(uname_m),x86_64)
                export arch:=intel64
        endif
        ifeq ($(uname_m),sparc64)
                export arch:=sparc
        endif
        ifndef arch
                export arch:=$(uname_m)
        endif
endif

ifndef runtime
        gcc_version = $(shell gcc -dumpversion)
        os_version:=$(shell uname -r)
        os_kernel_version:=$(shell uname -r | sed -e 's/-.*$$//')
        export os_glibc_version_full:=$(shell getconf GNU_LIBC_VERSION | grep glibc | sed -e 's/^glibc //')
        os_glibc_version:=$(shell echo "$(os_glibc_version_full)" | sed -e '2,$$d' -e 's/-.*$$//')
        export runtime:=cc$(gcc_version)_libc$(os_glibc_version)_kernel$(os_kernel_version)
endif

native_compiler := gcc
export compiler ?= gcc
debugger ?= gdb

CMD=sh -c
CWD=$(shell pwd)
CP=cp
RM?=rm -f
RD?=rmdir
MD?=mkdir -p
NUL= /dev/null
SLASH=/
MAKE_VERSIONS=sh $(tbb_root)/build/version_info_linux.sh $(CPLUS) $(CPLUS_FLAGS) $(INCLUDES) >version_string.ver
MAKE_TBBVARS=sh $(tbb_root)/build/generate_tbbvars.sh

ifdef LD_LIBRARY_PATH
        export LD_LIBRARY_PATH := .:$(LD_LIBRARY_PATH)
else
        export LD_LIBRARY_PATH := .
endif

####### Build settings ########################################################

OBJ = o
DLL = so
LIBEXT = so
SONAME_SUFFIX =$(shell grep TBB_COMPATIBLE_INTERFACE_VERSION $(tbb_root)/include/tbb/tbb_stddef.h | egrep -o [0-9.]+)

ifeq ($(arch),ia64)
        def_prefix = lin64ipf
endif
ifeq ($(arch),sparc)
        def_prefix = lin64
endif
ifeq (,$(def_prefix))
    ifeq (64,$(findstring 64,$(arch)))
            def_prefix = lin64
    else
            def_prefix = lin32
    endif
endif
TBB.LST = $(tbb_root)/src/tbb/$(def_prefix)-tbb-export.lst
TBB.DEF = $(TBB.LST:.lst=.def)

TBB.DLL = $(TBB_NO_VERSION.DLL).$(SONAME_SUFFIX)
TBB.LIB = $(TBB.DLL)
TBB_NO_VERSION.DLL=libtbb$(CPF_SUFFIX)$(DEBUG_SUFFIX).$(DLL)
LINK_TBB.LIB = $(TBB_NO_VERSION.DLL)

MALLOC_NO_VERSION.DLL = libtbbmalloc$(DEBUG_SUFFIX).$(DLL)
MALLOC.DEF = $(MALLOC_ROOT)/$(def_prefix)-tbbmalloc-export.def
MALLOC.DLL = $(MALLOC_NO_VERSION.DLL).$(SONAME_SUFFIX)
MALLOC.LIB = $(MALLOC_NO_VERSION.DLL)
LINK_MALLOC.LIB = $(MALLOC_NO_VERSION.DLL)

MALLOCPROXY_NO_VERSION.DLL = libtbbmalloc_proxy$(DEBUG_SUFFIX).$(DLL)
MALLOCPROXY.DEF = $(MALLOC_ROOT)/$(def_prefix)-proxy-export.def
MALLOCPROXY.DLL = $(MALLOCPROXY_NO_VERSION.DLL).$(SONAME_SUFFIX)
MALLOCPROXY.LIB = $(MALLOCPROXY_NO_VERSION.DLL)

RML_NO_VERSION.DLL = libirml$(DEBUG_SUFFIX).$(DLL)
RML.DEF = $(RML_SERVER_ROOT)/lin-rml-export.def
RML.DLL = $(RML_NO_VERSION.DLL).1
RML.LIB = $(RML_NO_VERSION.DLL)

TBB_NOSTRICT=1

TEST_LAUNCHER=sh $(tbb_root)/build/test_launcher.sh
