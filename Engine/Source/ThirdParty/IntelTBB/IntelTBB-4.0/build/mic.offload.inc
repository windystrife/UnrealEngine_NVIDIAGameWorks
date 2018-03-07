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

ifneq (mic,$(offload))
  $(error File mic.offload.inc should not be included directly. Use offload=mic instead.)
endif
ifneq (icc,$(compiler))
  $(error Only Intel(R) Compiler is supported for MIC offload compilation)
endif

tbb_offload_build_prefix?=mic_offload_icc
# The same build prefix should be used in mic.linux.inc
export tbb_mic_build_prefix?=mic_icc
initial_tbb_root:=$(tbb_root)

ifndef BUILDING_PHASE
tbb_native_debug:
	$(MAKE) tbb_debug target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)
tbb_native_release:
	$(MAKE) tbb_release target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)
tbbmalloc_native_debug:
	$(MAKE) tbbmalloc_debug target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)
tbbmalloc_native_release:
	$(MAKE) tbbmalloc_release target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)
rml_native_debug:
	$(MAKE) rml_debug target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)
rml_native_release:
	$(MAKE) rml_release target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)
tbb_native:
	$(MAKE) tbb target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)
tbbmalloc_native:
	$(MAKE) tbbmalloc target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)
rml_native:
	$(MAKE) rml target=mic compiler=icc offload= tbb_root=$(initial_tbb_root)

# The work_dir variable can not be set via target-specific variable mechanism
# since it will affect other targets on which the target depends.
# So work_dir is set via eval function inside the recipe.
prepare_test_offload_dir_debug: initial_tbb_build_dir:=$(tbb_build_dir)
prepare_test_offload_dir_debug: tbb_debug tbbmalloc_debug tbb_native_debug tbbmalloc_native_debug
	$(eval work_dir=$(initial_tbb_build_dir)$(SLASH)$(tbb_offload_build_prefix))
	$(shell $(MD) "$(work_dir)_debug$(SLASH)mic" >$(NUL) 2>$(NUL))
	$(shell $(CP) "$(initial_tbb_build_dir)$(SLASH)$(tbb_build_prefix)_debug$(SLASH)lib"* "$(work_dir)_debug" >$(NUL) 2>$(NUL))
	$(shell $(CP) "$(initial_tbb_build_dir)$(SLASH)$(tbb_mic_build_prefix)_debug$(SLASH)lib"* "$(work_dir)_debug$(SLASH)mic" >$(NUL) 2>$(NUL))
	$(eval export TEST_COMPILATION=1)

prepare_test_offload_dir_release: initial_tbb_build_dir:=$(tbb_build_dir)
prepare_test_offload_dir_release: tbb_release tbbmalloc_release tbb_native_release tbbmalloc_native_release
	$(eval work_dir=$(initial_tbb_build_dir)$(SLASH)$(tbb_offload_build_prefix))
	$(shell $(MD) "$(work_dir)_release$(SLASH)mic" >$(NUL) 2>$(NUL))
	$(shell $(CP) "$(initial_tbb_build_dir)$(SLASH)$(tbb_build_prefix)_release$(SLASH)lib"* "$(work_dir)_release" >$(NUL) 2>$(NUL))
	$(shell $(CP) "$(initial_tbb_build_dir)$(SLASH)$(tbb_mic_build_prefix)_release$(SLASH)lib"* "$(work_dir)_release$(SLASH)mic" >$(NUL) 2>$(NUL))
	$(eval export TEST_COMPILATION=1)

prepare_test_offload_dir: initial_tbb_build_dir:=$(tbb_build_dir)
prepare_test_offload_dir: tbb tbbmalloc tbb_native tbbmalloc_native
	$(eval work_dir=$(initial_tbb_build_dir)$(SLASH)$(tbb_offload_build_prefix))
	$(shell $(MD) "$(work_dir)_debug$(SLASH)mic" >$(NUL) 2>$(NUL))
	$(shell $(MD) "$(work_dir)_release$(SLASH)mic" >$(NUL) 2>$(NUL))
	$(shell $(CP) "$(initial_tbb_build_dir)$(SLASH)$(tbb_build_prefix)_debug$(SLASH)lib"* "$(work_dir)_debug" >$(NUL) 2>$(NUL))
	$(shell $(CP) "$(initial_tbb_build_dir)$(SLASH)$(tbb_build_prefix)_release$(SLASH)lib"* "$(work_dir)_release" >$(NUL) 2>$(NUL))
	$(shell $(CP) "$(initial_tbb_build_dir)$(SLASH)$(tbb_mic_build_prefix)_debug$(SLASH)lib"* "$(work_dir)_debug$(SLASH)mic" >$(NUL) 2>$(NUL))
	$(shell $(CP) "$(initial_tbb_build_dir)$(SLASH)$(tbb_mic_build_prefix)_release$(SLASH)lib"* "$(work_dir)_release$(SLASH)mic" >$(NUL) 2>$(NUL))
	$(eval export TEST_COMPILATION=1)

test: prepare_test_offload_dir
tbb_test_debug: prepare_test_offload_dir_debug
tbb_test_release: prepare_test_offload_dir_release
tbbmalloc_test_debug: prepare_test_offload_dir_debug
tbbmalloc_test_release: prepare_test_offload_dir_release
rml_test_debug: rml_native_debug prepare_test_offload_dir_debug
rml_test_release: rml_native_release prepare_test_offload_dir_release

debug_% test_% stress_% time_% perf_%:: work_dir:=$(tbb_build_dir)$(SLASH)$(tbb_offload_build_prefix)
debug_% test_% stress_% time_% perf_%:: export TEST_COMPILATION=1

clean: work_dir:=$(tbb_build_dir)$(SLASH)$(tbb_offload_build_prefix)
# The _clean* targets begin with an underscore symbol to prevent a collision with the clean_% template
clean: _clean_mic_subdir_debug _clean_mic_subdir_release _clean_host _clean_mic
clean_release: work_dir:=$(tbb_build_dir)$(SLASH)$(tbb_offload_build_prefix)
clean_release: _clean_mic_subdir_release _clean_host_release _clean_mic_release
clean_debug: work_dir:=$(tbb_build_dir)$(SLASH)$(tbb_offload_build_prefix)
clean_debug: _clean_mic_subdir_debug _clean_host_debug _clean_mic_debug
clean_%:: work_dir:=$(tbb_build_dir)$(SLASH)$(tbb_offload_build_prefix)

_clean_mic_subdir_debug:
	$(shell $(RM) $(work_dir)_debug$(SLASH)mic/$(SLASH)*.* >$(NUL) 2>$(NUL))
	$(shell $(RD) $(work_dir)_debug$(SLASH)mic >$(NUL) 2>$(NUL))
_clean_mic_subdir_release:
	$(shell $(RM) $(work_dir)_release$(SLASH)mic/$(SLASH)*.* >$(NUL) 2>$(NUL))
	$(shell $(RD) $(work_dir)_release$(SLASH)mic >$(NUL) 2>$(NUL))
_clean_host:
	$(MAKE) clean tbb_root=$(initial_tbb_root) offload=
_clean_mic:
	$(MAKE) clean target=mic tbb_root=$(initial_tbb_root) offload=
_clean_host_release:
	$(MAKE) clean_release tbb_root=$(initial_tbb_root) offload=
_clean_host_debug:
	$(MAKE) clean_debug tbb_root=$(initial_tbb_root) offload=
_clean_mic_release:
	$(MAKE) clean_release target=mic tbb_root=$(initial_tbb_root) offload=
_clean_mic_debug:
	$(MAKE) clean_debug target=mic tbb_root=$(initial_tbb_root) offload=
endif

ifeq ($(TEST_COMPILATION),1)
  ifdef LD_LIBRARY_PATH
    export MIC_LD_LIBRARY_PATH := ./mic:$(MIC_LD_LIBRARY_PATH)
  else
    export MIC_LD_LIBRARY_PATH := ./mic
  endif
  LINK_TBB.LIB=-offload-option,mic,ld,"./mic/$(TBB.LIB)" $(TBB.LIB)
  LINK_FLAGS =
  LIB_LINK_FLAGS += $(DYLIB_KEY)
  # Do not use -Werror because it is too strict for the early offload compiler.
  # Need to set anything because WARNING_AS_ERROR_KEY should not be empty.
  # Treat #2426 as a warning. Print errors only.
  tbb_strict=0
  override WARNING_AS_ERROR_KEY = Warning as error
  override WARNING_KEY = -diag-warning 2426 -w0
  # Offload compiler now has a switch that tells to compile the entire translation unit for offload,
  # and to skip the 'main' function.
  CXX_ONLY_FLAGS += -offload-attribute-target=mic
  # Enable mic-specific stuff.
  CXX_ONLY_FLAGS += -D__TBB_MIC=1
  CXX_ONLY_FLAGS += -offload-option,mic,compiler,"-D__TBB_MIC=1 -DHARNESS_INCOMPLETE_SOURCES=1 -D__TBB_MIC_NATIVE -DTBB_USE_EXCEPTIONS=0"
endif

# detects whether examples are being built.
ifeq ($(BUILDING_PHASE),0)
  export UI = con
  export x64 = 64
endif # examples
