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

ifndef tbb_os

  # Windows sets environment variable OS; for other systems, ask uname
  ifeq ($(OS),)
    OS:=$(shell uname)
    ifeq ($(OS),)
      $(error "Cannot detect operating system")
    endif
    export tbb_os=$(OS)
  endif

  ifeq ($(OS), Windows_NT)
    export tbb_os=windows
  endif
  ifeq ($(OS), Linux)
    export tbb_os=linux
  endif
  ifeq ($(OS), Darwin)
    export tbb_os=macos
  endif

endif # !tbb_os

ifeq ($(tbb_cpf),1)
  export CPF_SUFFIX ?=_preview
endif

ifeq (,$(wildcard $(tbb_root)/build/$(tbb_os).inc))
  $(error "$(tbb_os)" is not supported. Add build/$(tbb_os).inc file with os-specific settings )
endif

# detect arch and runtime versions, provide common host-specific definitions
include $(tbb_root)/build/$(tbb_os).inc

ifeq ($(arch),)
 $(error Architecture not detected)
endif
ifeq ($(runtime),)
 $(error Runtime version not detected)
endif

# process target-dependent compilation and testing configurations
ifdef target
 # optionally process target-dependent options for compilation and testing
 ifneq (,$(wildcard $(tbb_root)/build/$(target).inc))
  include $(tbb_root)/build/$(target).inc
 endif

 # optionally process host-dependent environment for target-dependent compilation and testing
 ifneq (,$(wildcard $(tbb_root)/build/$(target).$(tbb_os).inc))
  include $(tbb_root)/build/$(target).$(tbb_os).inc
 endif

 # insure at least one target-dependent configuration file was found for compilation and testing
 ifeq (,$(wildcard $(tbb_root)/build/$(target).inc)$(wildcard $(tbb_root)/build/$(target).$(tbb_os).inc))
  $(error "$(target)" is not supported. Add build/$(target).inc or build/$(target).$(tbb_os).inc file)
 endif
endif  #target

# Support for running debug tests to release library and vice versa
flip_cfg=$(subst _flipcfg,_release,$(subst _release,_debug,$(subst _debug,_flipcfg,$(1))))
cross_cfg = $(if $(crosstest),$(call flip_cfg,$(1)),$(1))

ifdef BUILDING_PHASE
 # Setting default configuration to release
 cfg?=release
 # No lambas or other C++0x extensions by default for compilers that implement them as experimental features
 # TODO: it should become unnecessary when all relevant tests are "moved" to the TEST_TBB_CPP11 set
 lambdas ?= 0
 cpp0x ?= 0

 ifndef target
  target:=$(tbb_os)
 endif
 # process host/target compiler-dependent build configuration
 ifeq (,$(wildcard $(tbb_root)/build/$(target).$(compiler).inc))
  $(error "$(compiler)" is not supported on $(target). Add build/$(target).$(compiler).inc file with compiler-specific settings. )
 endif
 include $(tbb_root)/build/$(target).$(compiler).inc
endif

ifneq ($(BUILDING_PHASE),1)
 # definitions for top-level Makefiles
 origin_build_dir:=$(origin tbb_build_dir)
 tbb_build_dir?=$(tbb_root)$(SLASH)build
 tbb_build_prefix?=$(tbb_os)_$(arch)_$(compiler)_$(runtime)
 work_dir=$(tbb_build_dir)$(SLASH)$(tbb_build_prefix)
endif  # BUILDING_PHASE != 1

ifdef offload
  extra_inc=$(offload).offload.inc
endif
ifdef extra_inc
 ifneq (,$(wildcard $(tbb_root)/build/$(extra_inc)))
  include $(tbb_root)/build/$(extra_inc)
 else
  $(error specified build file: "build/$(extra_inc)" is not found. )
 endif
endif

ifndef BUILDING_PHASE
  work_dir:=$(work_dir)
  # assign new value for tbb_root if path is not absolute (the filter keeps only /* paths)
  ifeq ($(filter /% $(SLASH)%, $(subst :, ,$(tbb_root)) ),)
   ifeq ($(origin_build_dir),undefined)
    override tbb_root:=../..
   else
    override tbb_root:=$(CWD)/$(tbb_root)
   endif
  endif
  export tbb_root
 endif # !BUILDING_PHASE

.DELETE_ON_ERROR:    # Make will delete target if error occurred when building it.

