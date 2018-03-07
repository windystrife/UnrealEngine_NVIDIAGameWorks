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
WARNING_KEY = -w1
DYLIB_KEY = -shared
EXPORT_KEY = -Wl,--version-script,
NOINTRINSIC_KEY = -fno-builtin
LIBDL = -ldl

TBB_NOSTRICT = 1

CPLUS = icpc
CONLY = icc

ITT_NOTIFY = -DDO_ITT_NOTIFY
ifeq (release,$(cfg))
CPLUS_FLAGS = $(ITT_NOTIFY) -O2 -g -DUSE_PTHREAD
else
CPLUS_FLAGS = $(ITT_NOTIFY) -O0 -g -DUSE_PTHREAD -DTBB_USE_DEBUG
endif

OPENMP_FLAG = -openmp
LIB_LINK_FLAGS = -shared -i-static -Wl,-soname=$(BUILDING_LIBRARY)
LIBS += -lpthread -lrt
C_FLAGS = $(CPLUS_FLAGS)
# ICC 11.0 and higher support -std=c++0x
ifneq (,$(shell icc -dumpversion | egrep  "^1[1-9]\."))
    CPP11_FLAGS = -std=c++0x -D_TBB_CPP0X
endif

# ICC 12.0 and higher provide Intel(R) Cilk Plus
ifneq (,$(shell icc -dumpversion | egrep  "^1[2-9]\."))
    CILK_AVAILABLE = yes
endif

TBB_ASM.OBJ=
MALLOC_ASM.OBJ=

ifeq (ia32,$(arch))
    CPLUS_FLAGS += -m32 -falign-stack=maintain-16-byte
    LIB_LINK_FLAGS += -m32
endif

ifeq (ia64,$(arch))
    ITT_NOTIFY =
# Position-independent code (PIC) is a must on IA-64, even for regular (not shared) executables
# strict-ansi does not work with <signal.h> on RHEL 4 AS
    CPLUS_FLAGS += $(PIC_KEY) $(if $(findstring cc3.,$(runtime)),-ansi,-strict-ansi)
else
    CPLUS_FLAGS += -strict-ansi
endif

ifneq (,$(codecov))
# no tool support for code coverage, need profile data generation
    ITT_NOTIFY = -prof-genx
endif

ifneq (00,$(lambdas)$(cpp0x))
	CPLUS_FLAGS += $(CPP11_FLAGS)
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

M_CPLUS_FLAGS = $(CPLUS_FLAGS) -fno-rtti -fno-exceptions

#------------------------------------------------------------------------------
# End of setting tbbmalloc data.
#------------------------------------------------------------------------------

