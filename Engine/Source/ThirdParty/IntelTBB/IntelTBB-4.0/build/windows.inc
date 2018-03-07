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

export SHELL = cmd

ifdef tbb_build_dir
  test_dir:=$(tbb_build_dir)
else
  test_dir:=.
endif

# TODO give an error if archs doesn't match
ifndef arch
  export arch:=$(shell cmd /C "cscript /nologo /E:jscript $(tbb_root)/build/detect.js /arch $(compiler)")
endif

ifndef runtime
  export runtime:=$(shell cmd /C "cscript /nologo /E:jscript $(tbb_root)/build/detect.js /runtime $(compiler)")
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

AR=lib
AR_OUTPUT_KEY=/out:
AR_FLAGS=/nologo /nodefaultlib

OBJ = obj
DLL = dll
LIBEXT = lib
ASMEXT = asm

def_prefix = $(if $(findstring ia32,$(arch)),win32,win64)

# Target Windows version. Do not increase beyond 0x0501 without prior discussion!
# Used as the value for macro definition opiton in windows.cl.inc etc.
_WIN32_WINNT=0x0501

TBB.LST = $(tbb_root)/src/tbb/$(def_prefix)-tbb-export.lst
TBB.DEF = $(TBB.LST:.lst=.def)
TBB.DLL = tbb$(CPF_SUFFIX)$(DEBUG_SUFFIX).$(DLL)
TBB.LIB = tbb$(CPF_SUFFIX)$(DEBUG_SUFFIX).$(LIBEXT)
TBB.RES = tbb_resource.res
# On Windows, we use #pragma comment to set the proper TBB lib to link with
# But for cross-configuration testing, need to link explicitly
LINK_TBB.LIB = $(if $(crosstest),$(TBB.LIB))
TBB.MANIFEST =
ifneq ($(filter vc8 vc9,$(runtime)),)
    TBB.MANIFEST = tbbmanifest.exe.manifest
endif

MALLOC.DEF = $(MALLOC_ROOT)/$(def_prefix)-tbbmalloc-export.def
MALLOC.DLL = tbbmalloc$(DEBUG_SUFFIX).$(DLL)
MALLOC.LIB = tbbmalloc$(DEBUG_SUFFIX).$(LIBEXT)
MALLOC.RES = tbbmalloc.res
MALLOC.MANIFEST =
ifneq ($(filter vc8 vc9,$(runtime)),)
MALLOC.MANIFEST = tbbmanifest.exe.manifest
endif
LINK_MALLOC.LIB = $(MALLOC.LIB)

MALLOCPROXY.DLL = tbbmalloc_proxy$(DEBUG_SUFFIX).$(DLL)
MALLOCPROXY.LIB = tbbmalloc_proxy$(DEBUG_SUFFIX).$(LIBEXT)

PROXY.LIB = tbbproxy$(DEBUG_SUFFIX).$(LIBEXT)

RML.DEF = $(RML_SERVER_ROOT)/$(def_prefix)-rml-export.def
RML.DLL = irml$(DEBUG_SUFFIX).$(DLL)
RML.LIB = irml$(DEBUG_SUFFIX).$(LIBEXT)
RML.RES = irml.res
ifneq ($(filter vc8 vc9,$(runtime)),)
RML.MANIFEST = tbbmanifest.exe.manifest
endif

MAKE_VERSIONS = cmd /C cscript /nologo /E:jscript $(subst \,/,$(tbb_root))/build/version_info_windows.js $(compiler) $(arch) $(subst \,/,"$(CPLUS) $(CPLUS_FLAGS)") > version_string.ver
MAKE_TBBVARS  = cmd /C "$(subst /,\,$(tbb_root))\build\generate_tbbvars.bat"

TEST_LAUNCHER =  $(subst /,\,$(tbb_root))\build\test_launcher.bat
