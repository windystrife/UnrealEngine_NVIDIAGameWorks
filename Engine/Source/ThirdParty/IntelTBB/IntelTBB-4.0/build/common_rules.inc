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

ifeq ($(tbb_strict),1)
  ifeq ($(WARNING_AS_ERROR_KEY),)
    $(error WARNING_AS_ERROR_KEY is empty)
  endif
  # Do not remove line below!
  WARNING_KEY += $(WARNING_AS_ERROR_KEY)
endif

ifndef TEST_EXT
    TEST_EXT = exe
endif

.PRECIOUS: %.$(OBJ) %.$(TEST_EXT) %.res

INCLUDES += $(INCLUDE_KEY)$(tbb_root)/src $(INCLUDE_KEY)$(tbb_root)/src/rml/include $(INCLUDE_KEY)$(tbb_root)/include

CPLUS_FLAGS += $(WARNING_KEY) $(CXXFLAGS)
ifeq ($(tbb_cpf),1)
CPLUS_FLAGS += $(DEFINE_KEY)__TBB_CPF_BUILD=1
endif
LINK_FLAGS += $(LDFLAGS)
LIB_LINK_FLAGS += $(LDFLAGS)
CPLUS_FLAGS_NOSTRICT = $(subst -strict-ansi,-ansi,$(CPLUS_FLAGS))

LIB_LINK_CMD ?= $(CPLUS) $(PIC_KEY)
ifeq ($(origin LIB_OUTPUT_KEY), undefined)
    LIB_OUTPUT_KEY = $(OUTPUT_KEY)
endif
ifeq ($(origin LIB_LINK_LIBS), undefined)
    LIB_LINK_LIBS = $(LIBDL) $(LIBS)
endif

CONLY ?= $(CPLUS)

# The most generic rules
%.$(OBJ): %.cpp
	$(CPLUS) $(COMPILE_ONLY) $(CPLUS_FLAGS) $(CXX_ONLY_FLAGS) $(CXX_WARN_SUPPRESS) $(INCLUDES) $<

%.$(OBJ): %.c
	$(CONLY) $(COMPILE_ONLY) $(C_FLAGS) $(INCLUDES) $<

%.$(OBJ): %.asm
	$(ASM) $(ASM_FLAGS) $<

%.$(OBJ): %.s
	cpp <$< | grep -v '^#' >$*.tmp
	$(ASM) $(ASM_FLAGS) -o $@ $*.tmp

# Rule for generating .E file if needed for visual inspection
# Note that due to mapping for ICL all uses of PREPROC_ONLY should be immediately followed by a file name
%.E: %.cpp
	$(CPLUS) $(CPLUS_FLAGS) $(CXX_ONLY_FLAGS) $(INCLUDES) $(PREPROC_ONLY) $< >$@

# TODO Rule for generating .asm file if needed for visual inspection
%.asm: %.cpp
	$(CPLUS) /c /FAs /Fa $(CPLUS_FLAGS) $(CXX_ONLY_FLAGS) $(INCLUDES) $<

# TODO Rule for generating .s file if needed for visual inspection
%.s: %.cpp
	$(CPLUS) -S $(CPLUS_FLAGS) $(CXX_ONLY_FLAGS) $(INCLUDES) $<

# Customizations

ifeq (1,$(TBB_NOSTRICT))
# GNU 3.2.3 headers have a ISO syntax that is rejected by Intel compiler in -strict-ansi mode.
# The Mac uses gcc, so the list is empty for that platform.
# The files below need the -strict-ansi flag downgraded to -ansi to compile

$(KNOWN_NOSTRICT): %.$(OBJ): %.cpp
	$(CPLUS) $(COMPILE_ONLY) $(CPLUS_FLAGS_NOSTRICT) $(CXX_ONLY_FLAGS) $(CXX_WARN_SUPPRESS) $(INCLUDES) $<
endif

$(KNOWN_WARNINGS): %.$(OBJ): %.cpp
	$(CPLUS) $(COMPILE_ONLY) $(subst $(WARNING_KEY),,$(CPLUS_FLAGS)) $(CXX_ONLY_FLAGS) $(CXX_WARN_SUPPRESS) $(INCLUDES) $<

tbb_misc.$(OBJ): tbb_misc.cpp version_string.ver
	$(CPLUS) $(COMPILE_ONLY) $(CPLUS_FLAGS) $(CXX_ONLY_FLAGS) $(CXX_WARN_SUPPRESS) $(INCLUDE_KEY). $(INCLUDES) $<

tbb_misc.E: tbb_misc.cpp version_string.ver
	$(CPLUS) $(CPLUS_FLAGS) $(CXX_ONLY_FLAGS) $(INCLUDE_KEY). $(INCLUDES) $(PREPROC_ONLY) $< >$@

%.res: %.rc version_string.ver $(TBB.MANIFEST)
	rc /Fo$@ $(INCLUDES) $(filter /D%,$(CPLUS_FLAGS)) $<

ifneq (,$(TBB.MANIFEST))
$(TBB.MANIFEST):
	cmd /C "echo #include ^<stdio.h^> >tbbmanifest.c"
	cmd /C "echo int main(){return 0;} >>tbbmanifest.c"
	cl /nologo $(C_FLAGS) tbbmanifest.c

version_string.ver: $(TBB.MANIFEST)
	$(MAKE_VERSIONS)
	cmd /C "echo #define TBB_MANIFEST 1 >> version_string.ver"

else
version_string.ver:
	$(MAKE_VERSIONS)
endif


# Rules for generating a test DLL
%_dll.$(OBJ): %.cpp
	$(CPLUS) $(COMPILE_ONLY) $(OUTPUTOBJ_KEY)$@ $(CPLUS_FLAGS) $(PIC_KEY) $(DEFINE_KEY)_USRDLL $(INCLUDES) $<
