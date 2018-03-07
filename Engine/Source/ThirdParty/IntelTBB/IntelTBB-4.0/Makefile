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

tbb_root?=.
include $(tbb_root)/build/common.inc
.PHONY: default all tbb tbbmalloc tbbproxy test examples

#workaround for non-depend targets tbb and tbbmalloc which both depend on version_string.ver
#According to documentation submakes should run in parallel
.NOTPARALLEL: tbb tbbmalloc tbbproxy

default: tbb tbbmalloc $(if $(use_proxy),tbbproxy)

all: tbb tbbmalloc tbbproxy test examples

tbb: mkdir
	$(MAKE) -C "$(work_dir)_debug"  -r -f $(tbb_root)/build/Makefile.tbb cfg=debug tbb_root=$(tbb_root)
	$(MAKE) -C "$(work_dir)_release"  -r -f $(tbb_root)/build/Makefile.tbb cfg=release tbb_root=$(tbb_root)

tbbmalloc: mkdir
	$(MAKE) -C "$(work_dir)_debug"  -r -f $(tbb_root)/build/Makefile.tbbmalloc cfg=debug malloc tbb_root=$(tbb_root)
	$(MAKE) -C "$(work_dir)_release"  -r -f $(tbb_root)/build/Makefile.tbbmalloc cfg=release malloc tbb_root=$(tbb_root)

tbbproxy: mkdir
	$(MAKE) -C "$(work_dir)_debug"  -r -f $(tbb_root)/build/Makefile.tbbproxy cfg=debug tbbproxy tbb_root=$(tbb_root)
	$(MAKE) -C "$(work_dir)_release"  -r -f $(tbb_root)/build/Makefile.tbbproxy cfg=release tbbproxy tbb_root=$(tbb_root)

test: tbb tbbmalloc $(if $(use_proxy),tbbproxy)
	-$(MAKE) -C "$(work_dir)_debug"  -r -f $(tbb_root)/build/Makefile.tbbmalloc cfg=debug malloc_test tbb_root=$(tbb_root)
	-$(MAKE) -C "$(work_dir)_debug"  -r -f $(tbb_root)/build/Makefile.test cfg=debug tbb_root=$(tbb_root)
	-$(MAKE) -C "$(work_dir)_release"  -r -f $(tbb_root)/build/Makefile.tbbmalloc cfg=release malloc_test tbb_root=$(tbb_root)
	-$(MAKE) -C "$(work_dir)_release"  -r -f $(tbb_root)/build/Makefile.test cfg=release tbb_root=$(tbb_root)

rml: mkdir
	$(MAKE) -C "$(work_dir)_debug"  -r -f $(tbb_root)/build/Makefile.rml cfg=debug tbb_root=$(tbb_root)
	$(MAKE) -C "$(work_dir)_release"  -r -f $(tbb_root)/build/Makefile.rml cfg=release tbb_root=$(tbb_root)


examples: tbb tbbmalloc
	$(MAKE) -C examples -r -f Makefile tbb_root=.. release test

.PHONY: clean clean_examples mkdir info

clean: clean_examples
	$(shell $(RM) $(work_dir)_release$(SLASH)*.* >$(NUL) 2>$(NUL))
	$(shell $(RD) $(work_dir)_release >$(NUL) 2>$(NUL))
	$(shell $(RM) $(work_dir)_debug$(SLASH)*.* >$(NUL) 2>$(NUL))
	$(shell $(RD) $(work_dir)_debug >$(NUL) 2>$(NUL))
	@echo clean done

clean_examples:
	$(shell $(MAKE) -s -i -r -C examples -f Makefile tbb_root=.. clean >$(NUL) 2>$(NUL))

mkdir:
	$(shell $(MD) "$(work_dir)_release" >$(NUL) 2>$(NUL))
	$(shell $(MD) "$(work_dir)_debug" >$(NUL) 2>$(NUL))
	@echo Created $(work_dir)_release and ..._debug directories

info:
	@echo OS: $(tbb_os)
	@echo arch=$(arch)
	@echo compiler=$(compiler)
	@echo runtime=$(runtime)
	@echo tbb_build_prefix=$(tbb_build_prefix)

