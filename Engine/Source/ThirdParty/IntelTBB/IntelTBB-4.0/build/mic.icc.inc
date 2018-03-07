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

ifeq (release,$(cfg))
    CPLUS_FLAGS = -O2 -g -DUSE_PTHREAD
else
    CPLUS_FLAGS = -O0 -g -DUSE_PTHREAD -DTBB_USE_DEBUG
endif

ifneq (,$(codecov))
    CPLUS_FLAGS += -prof-genx
endif

OPENMP_FLAG = -openmp
LIB_LINK_FLAGS = -shared -i-static -Wl,-soname=$(BUILDING_LIBRARY)
LIBS += -lpthread -lrt
C_FLAGS = $(CPLUS_FLAGS)
CPP11_FLAGS = -std=c++0x -D_TBB_CPP0X
CILK_AVAILABLE = yes

TBB_ASM.OBJ=
MALLOC_ASM.OBJ=

CPLUS_FLAGS += -strict-ansi

ifneq (00,$(lambdas)$(cpp0x))
    CPLUS_FLAGS += $(CPP11_FLAGS)
endif

CPLUS_FLAGS += -DHARNESS_INCOMPLETE_SOURCES=1 -D__TBB_MIC_NATIVE -DTBB_USE_EXCEPTIONS=0
CPLUS += -mmic
CONLY += -mmic
LINK_FLAGS = -Wl,-rpath-link=.
# Tell the icc to not link against libcilk*. Otherwise icc tries to link and emits a warning message.
LIB_LINK_LIBS += -no-intel-extensions
# Do not depend on libirc etc dynamic libs. It makes 'native' execution easier for the users.
LIB_LINK_LIBS += -static-intel

#------------------------------------------------------------------------------
# Setting tbbmalloc data.
#------------------------------------------------------------------------------

M_CPLUS_FLAGS = $(CPLUS_FLAGS) -fno-rtti -fno-exceptions

#------------------------------------------------------------------------------
# End of setting tbbmalloc data.
#------------------------------------------------------------------------------



