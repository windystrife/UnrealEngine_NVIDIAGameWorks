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

ifdef tbb_build_dir
  test_dir:=$(tbb_build_dir)
else
  test_dir:=.
endif

# TODO give an error if archs doesn't match
ifndef arch
  export arch:=xbox360
endif

ifndef runtime
  export runtime:=xdk
endif

native_compiler := cl
export compiler ?= cl
debugger ?= devenv /debugexe

CMD=cmd /C
CWD=$(shell cmd /C echo %CD%)
RM=cmd /C del /Q /F
RD=cmd /C rmdir
MD=cmd /c mkdir
SLASH=\\
NUL = nul

OBJ = obj
DLL = dll
LIBEXT = lib

def_prefix = $(arch)

# Target Windows version. Do not increase beyond 0x0500 without prior discussion!
# Used as the value for macro definition opiton in compiler specific inc files.
_WIN32_WINNT=0x0400

TBB.LST =
TBB.DEF = $(tbb_root)/src/tbb/$(def_prefix)-tbb-export.def
TBB.DLL = tbb$(CPF_SUFFIX)$(DEBUG_SUFFIX).$(DLL)
TBB.LIB = tbb$(CPF_SUFFIX)$(DEBUG_SUFFIX).$(LIBEXT)
TBB.RES =
#On Windows we specify appropriate tbb library using #pragma comment
LINK_TBB.LIB =

MALLOC.DEF = $(MALLOC_ROOT)/$(def_prefix)-tbbmalloc-export.def
MALLOC.DLL = tbbmalloc$(DEBUG_SUFFIX).$(DLL)
MALLOC.LIB = tbbmalloc$(DEBUG_SUFFIX).$(LIBEXT)
MALLOC.RES =

MAKE_VERSIONS = cmd /C cscript /nologo /E:jscript $(subst \,/,$(tbb_root))/build/version_info_windows.js $(compiler) $(arch) $(subst \,/,"$(CPLUS) $(CPLUS_FLAGS) $(INCLUDES)") > version_string.ver
MAKE_TBBVARS  = cmd /C "$(subst /,\,$(tbb_root))\build\generate_tbbvars.bat"
