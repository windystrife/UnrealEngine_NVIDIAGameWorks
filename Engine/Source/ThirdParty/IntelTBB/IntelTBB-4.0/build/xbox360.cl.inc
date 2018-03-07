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
# Define compiler-specific variables.
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
# Setting compiler flags.
#------------------------------------------------------------------------------
CPLUS = cl /nologo
LINK_FLAGS = /link /nologo
LIB_LINK_FLAGS=/link /nologo /DLL /MAP /DEBUG
MS_CRT_KEY = /MT$(if $(findstring debug,$(cfg)),d)
EH_FLAGS = /EHsc /GR
               
ifeq ($(cfg), release)
        CPLUS_FLAGS = $(MS_CRT_KEY) /O2 /Zi $(EH_FLAGS) /Zc:forScope /D_XBOX /DTBB_NO_LEGACY=1
        ASM_FLAGS =
endif
ifeq ($(cfg), debug)
        CPLUS_FLAGS = $(MS_CRT_KEY) /Od /Ob0 /Zi $(EH_FLAGS) /Zc:forScope \
                      /DTBB_USE_DEBUG /D_XBOX /DTBB_NO_LEGACY=1
        ASM_FLAGS = /DUSE_FRAME_POINTER
endif


COMPILE_ONLY = /c
PREPROC_ONLY = /TP /EP
INCLUDE_KEY = /I
DEFINE_KEY = /D
OUTPUT_KEY = /Fe
OUTPUTOBJ_KEY = /Fo
WARNING_AS_ERROR_KEY = /WX
WARNING_KEY = /W3
DYLIB_KEY = /DLL
EXPORT_KEY = /DEF:

ifeq (em64t,$(arch))
        CPLUS_FLAGS += /GS-
endif

CPLUS_FLAGS += /DUSE_WINTHREAD /D_CRT_SECURE_NO_DEPRECATE /D_WIN32_WINNT=$(_WIN32_WINNT)
C_FLAGS = $(CPLUS_FLAGS) /TC
#------------------------------------------------------------------------------
# End of setting compiler flags.
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
# Setting assembler data.
#------------------------------------------------------------------------------
# nothing for XBOX360
#------------------------------------------------------------------------------
# End of setting assembler data.
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
# Setting tbbmalloc data.
#------------------------------------------------------------------------------
M_CPLUS_FLAGS = $(subst $(EH_FLAGS),/EHs-,$(CPLUS_FLAGS))
#------------------------------------------------------------------------------
# End of setting tbbmalloc data.
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# End of define compiler-specific variables.
#------------------------------------------------------------------------------
