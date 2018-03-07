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

CPLUS = clang++
CONLY = clang
COMPILE_ONLY = -c -MMD
PREPROC_ONLY = -E -x c++
INCLUDE_KEY = -I
DEFINE_KEY = -D
OUTPUT_KEY = -o #
OUTPUTOBJ_KEY = -o #
PIC_KEY = -fPIC
WARNING_AS_ERROR_KEY = -Werror
WARNING_KEY = -Wall
TEST_WARNING_KEY = -Wextra -Wshadow -Wcast-qual -Woverloaded-virtual -Wnon-virtual-dtor
WARNING_SUPPRESS = -Wno-non-virtual-dtor
DYLIB_KEY = -dynamiclib
EXPORT_KEY = -Wl,-exported_symbols_list,
LIBDL = -ldl

LIBS = -lpthread
LINK_FLAGS =
LIB_LINK_FLAGS = -dynamiclib
C_FLAGS = $(CPLUS_FLAGS) -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk

ifeq ($(cfg), release)
    CPLUS_FLAGS = -O2
else
    CPLUS_FLAGS = -g -O0 -DTBB_USE_DEBUG
endif

CPLUS_FLAGS += -DUSE_PTHREAD

ifeq (intel64,$(arch))
    CPLUS_FLAGS += -m64
    LINK_FLAGS += -m64
    LIB_LINK_FLAGS += -m64
endif

ifeq (ia32,$(arch))
    CPLUS_FLAGS += -m32
    LINK_FLAGS += -m32
    LIB_LINK_FLAGS += -m32
endif

ifeq (ppc64,$(arch))
    CPLUS_FLAGS += -arch ppc64
    LINK_FLAGS += -arch ppc64
    LIB_LINK_FLAGS += -arch ppc64
endif

ifeq (ppc32,$(arch))
    CPLUS_FLAGS += -arch ppc
    LINK_FLAGS += -arch ppc
    LIB_LINK_FLAGS += -arch ppc
endif

#------------------------------------------------------------------------------
# Setting assembler data.
#------------------------------------------------------------------------------

ASM = as
ifeq (intel64,$(arch))
    ASM_FLAGS += -arch x86_64
endif
ifeq (ia32,$(arch))
    ASM_FLAGS += -arch i386
endif
ifeq ($(cfg), debug)
    ASM_FLAGS += -g
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

