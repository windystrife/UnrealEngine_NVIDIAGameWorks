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

ifndef arch
        arch:=$(shell uname -p)
        ifeq ($(arch),i386)
            ifeq ($(shell isainfo -b),64)
                arch:=intel64
            else
                arch:=ia32
            endif
        endif
        export arch
# For non-IA systems running Sun OS, 'arch' will contain whatever is printed by uname -p.
# In particular, for SPARC architecture it will contain "sparc".
endif

ifndef runtime
        gcc_version:=$(shell gcc -v 2>&1 | grep 'gcc version' | sed -e 's/^gcc version //' | sed -e 's/ .*$$//')
        os_version:=$(shell uname -r)
        os_kernel_version:=$(shell uname -r | sed -e 's/-.*$$//')
        export runtime:=cc$(gcc_version)_kernel$(os_kernel_version)
endif

ifeq ($(arch),sparc)
        native_compiler := gcc
        export compiler ?= gcc
else
        native_compiler := suncc
        export compiler ?= suncc
endif
# debugger ?= gdb

CMD=$(SHELL) -c
CWD=$(shell pwd)
RM?=rm -f
RD?=rmdir
MD?=mkdir -p
NUL= /dev/null
SLASH=/
MAKE_VERSIONS=bash $(tbb_root)/build/version_info_sunos.sh $(CPLUS) $(CPLUS_FLAGS) $(INCLUDES) >version_string.ver
MAKE_TBBVARS=bash $(tbb_root)/build/generate_tbbvars.sh

ifdef LD_LIBRARY_PATH
        export LD_LIBRARY_PATH := .:$(LD_LIBRARY_PATH)
else
        export LD_LIBRARY_PATH := .
endif

####### Build settings ########################################################

OBJ = o
DLL = so
LIBEXT=so

TBB.LST =
TBB.DEF =
TBB.DLL = libtbb$(CPF_SUFFIX)$(DEBUG_SUFFIX).$(DLL)
TBB.LIB = $(TBB.DLL)
LINK_TBB.LIB = $(TBB.LIB)

MALLOC.DLL = libtbbmalloc$(DEBUG_SUFFIX).$(DLL)
MALLOC.LIB = $(MALLOC.DLL)

MALLOCPROXY.DLL = libtbbmalloc_proxy$(DEBUG_SUFFIX).$(DLL)

TBB_NOSTRICT=1

TEST_LAUNCHER=sh $(tbb_root)/build/test_launcher.sh
