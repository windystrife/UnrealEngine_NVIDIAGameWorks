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

COMPILE_ONLY = -c -MMD
PREPROC_ONLY = -E -x c++
INCLUDE_KEY = -I
DEFINE_KEY = -D
OUTPUT_KEY = -o #
OUTPUTOBJ_KEY = -o #
PIC_KEY = -fPIC
WARNING_AS_ERROR_KEY = -Werror
WARNING_KEY = -Wall
TEST_WARNING_KEY = -Wshadow -Wcast-qual -Woverloaded-virtual -Wnon-virtual-dtor $(if $(findstring cc4., $(runtime)),-Wextra)

WARNING_SUPPRESS = -Wno-parentheses -Wno-non-virtual-dtor
DYLIB_KEY = -shared
EXPORT_KEY = -Wl,--version-script,
LIBDL = -ldl

TBB_NOSTRICT = 1

CPLUS = g++
CONLY = gcc
LIB_LINK_FLAGS = $(DYLIB_KEY) -Wl,-soname=$(BUILDING_LIBRARY)
LIBS += -lpthread -lrt
LINK_FLAGS = -Wl,-rpath-link=.
C_FLAGS = $(CPLUS_FLAGS)
# gcc 4.4 and higher support -std=c++0x
ifneq (,$(shell gcc -dumpversion | egrep  "^(4\.[4-9]|[5-9])"))
    CPP11_FLAGS = -std=c++0x -D_TBB_CPP0X
endif

# gcc 4.2 and higher support OpenMP
ifneq (,$(shell gcc -dumpversion | egrep  "^(4\.[2-9]|[5-9])"))
    OPENMP_FLAG = -fopenmp
endif

ITT_NOTIFY = -DDO_ITT_NOTIFY
ifeq ($(cfg), release)
        CPLUS_FLAGS = $(ITT_NOTIFY) -g -O2 -DUSE_PTHREAD
endif
ifeq ($(cfg), debug)
        CPLUS_FLAGS = -DTBB_USE_DEBUG $(ITT_NOTIFY) -g -O0 -DUSE_PTHREAD
endif

ifneq (00,$(lambdas)$(cpp0x))
    CXX_ONLY_FLAGS += $(CPP11_FLAGS)
endif

TBB_ASM.OBJ=
MALLOC_ASM.OBJ=

ifeq (ia64,$(arch))
    ITT_NOTIFY =
# Position-independent code (PIC) is a must on IA-64, even for regular (not shared) executables
    CPLUS_FLAGS += $(PIC_KEY)
endif

ifeq (intel64,$(arch))
    CPLUS_FLAGS += -m64
    LIB_LINK_FLAGS += -m64
endif

ifeq (ia32,$(arch))
    CPLUS_FLAGS += -m32 -march=pentium4
    LIB_LINK_FLAGS += -m32
endif

ifeq (ppc64,$(arch))
    CPLUS_FLAGS += -m64
    LIB_LINK_FLAGS += -m64
endif

ifeq (ppc32,$(arch))
    CPLUS_FLAGS += -m32
    LIB_LINK_FLAGS += -m32
endif

ifeq (bgp,$(arch))
    CPLUS = mpicxx
    CONLY = mpicc
endif

# for some gcc versions on Solaris, -m64 may imply V9, but perhaps not everywhere (TODO: verify)
ifeq (sparc,$(arch))
    CPLUS_FLAGS    += -mcpu=v9 -m64
    LIB_LINK_FLAGS += -mcpu=v9 -m64
endif

#------------------------------------------------------------------------------
# Setting assembler data.
#------------------------------------------------------------------------------
ASM = as
ifeq (intel64,$(arch))
    ASM_FLAGS += --64
endif
ifeq (ia32,$(arch))
    ASM_FLAGS += --32
endif
ifeq ($(cfg),debug)
    ASM_FLAGS += -g
endif

ASSEMBLY_SOURCE=$(arch)-gas
ifeq (ia64,$(arch))
    ASM_FLAGS += -xexplicit
    TBB_ASM.OBJ += atomic_support.o lock_byte.o log2.o pause.o ia64_misc.o
    MALLOC_ASM.OBJ += atomic_support.o lock_byte.o pause.o log2.o
endif
#------------------------------------------------------------------------------
# End of setting assembler data.
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Setting tbbmalloc data.
#------------------------------------------------------------------------------

M_CPLUS_FLAGS = $(CPLUS_FLAGS) -fno-rtti -fno-exceptions -fno-schedule-insns2

#------------------------------------------------------------------------------
# End of setting tbbmalloc data.
#------------------------------------------------------------------------------
