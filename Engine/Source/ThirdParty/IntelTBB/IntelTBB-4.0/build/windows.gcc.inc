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

#------------------------------------------------------------------------------
# Overriding settings from windows.inc
#------------------------------------------------------------------------------

SLASH= $(strip \)
OBJ = o
LIBEXT = dll # MinGW allows linking with DLLs directly

TBB.RES =
MALLOC.RES =
RML.RES =
TBB.MANIFEST =
MALLOC.MANIFEST =
RML.MANIFEST =

ifeq (ia32,$(arch))
    TBB.LST = $(tbb_root)/src/tbb/lin32-tbb-export.lst
else
    TBB.LST = $(tbb_root)/src/tbb/win64-gcc-tbb-export.lst
endif
MALLOC.DEF = $(MALLOC_ROOT)/$(def_prefix)-gcc-tbbmalloc-export.def
RML.DEF = $(RML_SERVER_ROOT)/lin-rml-export.def

LINK_TBB.LIB = $(TBB.LIB)

#------------------------------------------------------------------------------
# End of overridden settings
#------------------------------------------------------------------------------
# Compiler-specific variables
#------------------------------------------------------------------------------

CPLUS = g++
COMPILE_ONLY = -c -MMD
PREPROC_ONLY = -E -x c++
INCLUDE_KEY = -I
DEFINE_KEY = -D
OUTPUT_KEY = -o #
OUTPUTOBJ_KEY = -o #
PIC_KEY =
WARNING_AS_ERROR_KEY = -Werror
WARNING_KEY = -Wall
TEST_WARNING_KEY = -Wextra -Wshadow -Wcast-qual -Woverloaded-virtual -Wnon-virtual-dtor -Wno-uninitialized
WARNING_SUPPRESS = -Wno-parentheses -Wno-uninitialized
DYLIB_KEY = -shared
LIBDL =
EXPORT_KEY = -Wl,--version-script,
LIBS = -lpsapi

#------------------------------------------------------------------------------
# End of compiler-specific variables
#------------------------------------------------------------------------------
# Command lines
#------------------------------------------------------------------------------

LINK_FLAGS = -Wl,--enable-auto-import
LIB_LINK_FLAGS = $(DYLIB_KEY)
# gcc 4.4 and higher support -std=c++0x
ifeq (ok,$(shell cmd /C "cscript /nologo /E:jscript $(tbb_root)/build/detect.js /minversion gcc 4.4"))
    CPP11_FLAGS = -std=c++0x -D_TBB_CPP0X
endif

ifeq ($(cfg), release)
        CPLUS_FLAGS = -O2
endif
ifeq ($(cfg), debug)
        CPLUS_FLAGS = -g -O0 -DTBB_USE_DEBUG
endif
CPLUS_FLAGS += -DUSE_WINTHREAD
CPLUS_FLAGS += -D_WIN32_WINNT=$(_WIN32_WINNT)

# MinGW specific
CPLUS_FLAGS += -DMINGW_HAS_SECURE_API=1 -D__MSVCRT_VERSION__=0x0700 -msse -mthreads

CONLY = gcc
debugger = gdb
C_FLAGS = $(CPLUS_FLAGS)

ifeq (intel64,$(arch))
    CPLUS_FLAGS += -m64
    LIB_LINK_FLAGS += -m64
endif

ifeq (ia32,$(arch))
    CPLUS_FLAGS += -m32 -march=i686
    LIB_LINK_FLAGS += -m32
endif

# For examples
export UNIXMODE = 1

#------------------------------------------------------------------------------
# End of command lines
#------------------------------------------------------------------------------
# Setting assembler data
#------------------------------------------------------------------------------

ASM=
ASM_FLAGS=
TBB_ASM.OBJ=
ASSEMBLY_SOURCE=$(arch)-gas

#------------------------------------------------------------------------------
# End of setting assembler data
#------------------------------------------------------------------------------
# Setting tbbmalloc data
#------------------------------------------------------------------------------

M_CPLUS_FLAGS = $(CPLUS_FLAGS) -fno-rtti -fno-exceptions

#------------------------------------------------------------------------------
# End of setting tbbmalloc data
#------------------------------------------------------------------------------
