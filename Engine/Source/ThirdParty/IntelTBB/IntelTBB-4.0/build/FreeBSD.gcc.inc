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
DYLIB_KEY = -shared

TBB_NOSTRICT = 1

CPLUS = g++ 
CONLY = gcc
LIB_LINK_FLAGS = -shared
LIBS = -lpthread 
C_FLAGS = $(CPLUS_FLAGS)

ifeq ($(cfg), release)
        CPLUS_FLAGS = -O2 -DUSE_PTHREAD
endif
ifeq ($(cfg), debug)
        CPLUS_FLAGS = -DTBB_USE_DEBUG -g -O0 -DUSE_PTHREAD
endif

ASM=
ASM_FLAGS=

TBB_ASM.OBJ=
MALLOC_ASM.OBJ=

ifeq (ia64,$(arch))
# Position-independent code (PIC) is a must on IA-64, even for regular (not shared) executables
    CPLUS_FLAGS += $(PIC_KEY)
endif 

ifeq (intel64,$(arch))
    CPLUS_FLAGS += -m64
    LIB_LINK_FLAGS += -m64
endif 

ifeq (ia32,$(arch))
    CPLUS_FLAGS += -m32
    LIB_LINK_FLAGS += -m32
endif 

#------------------------------------------------------------------------------
# Setting assembler data.
#------------------------------------------------------------------------------
ASSEMBLY_SOURCE=$(arch)-gas
ifeq (ia64,$(arch))
    ASM=as
    TBB_ASM.OBJ = atomic_support.o lock_byte.o log2.o pause.o
    MALLOC_ASM.OBJ = atomic_support.o lock_byte.o pause.o
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
