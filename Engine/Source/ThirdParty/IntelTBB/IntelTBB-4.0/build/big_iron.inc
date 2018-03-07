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
# Defines settings for building the TBB run-time as a static library.
# Use these only on platforms where dynamic linking is impractical.
#
# IF YOU USE TBB AS A STATIC LIBRARY, YOU MUST GUARANTEE THAT ONLY ONE COPY OF 
# THE TBB RUN-TIME IS LINKED INTO AN APPLICATION!  LINKING IN MULTIPLE COPIES
# OF THE TBB RUN-TIME, DIRECTLY OR INDIRECTLY, MAY CAUSE PROGRAM FAILURE!
#------------------------------------------------------------------------------

# Note that ITT_NOTIFY allows to selectively remove the definition of
# DO_ITT_NOTIFY without sabotaging deferred expansion of CPLUS_FLAGS.
# TODO: currently only in linux.{gcc,xl}.inc

# Note that -pthread with xl gives "1501-210 (W) command option t contains an incorrect subargument";
# multithreading is instead achieved by using the _r affix in the compiler name.
# TODO: is -lpthread still relevant/needed with XL and _r affix?

# Note that -D__TBB_NO_DLOPEN is only for scalability,
# and is not necessarily needed by any static build.

# OS specific settings =>
    LIB_LINK_CMD = ar rcs
    LIB_LINK_FLAGS =
    LIB_LINK_LIBS =
    LIB_OUTPUT_KEY =
    ifeq ($(tbb_os),linux)
        ifeq ($(compiler),gcc)
            LIBS = -pthread -ldl -lrt
        endif
        ifeq ($(compiler),xl)
            LIBS = -lpthread -ldl -lrt
        endif
        LINK_FLAGS =
    endif
    override CXXFLAGS += -D__TBB_DYNAMIC_LOAD_ENABLED=0 -D__TBB_SOURCE_DIRECTLY_INCLUDED=1
    ITT_NOTIFY =
    LIBEXT = a
    LIBPREF = lib
# <= OS specific settings

TBB.DLL = $(LIBPREF)tbb$(DEBUG_SUFFIX).$(LIBEXT)
TBB.LST =
TBB.DEF =
MALLOC.DLL = $(LIBPREF)tbbmalloc$(DEBUG_SUFFIX).$(LIBEXT)
MALLOC.DEF =
MALLOCPROXY.DLL =
MALLOCPROXY.DEF =
LINK_TBB.LIB = $(TBB.DLL)
