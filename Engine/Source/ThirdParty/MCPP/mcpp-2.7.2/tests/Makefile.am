## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = dejagnu foreign

if REPLACE_CPP

Makefile:

check-DEJAGNU:
	EXPECT=$(EXPECT); export EXPECT

	@if test x"$(gcc_testsuite_dir)" = x""; then \
        echo "ERROR: testsuite directory is not specified.  Do './configure --with-gcc-testsuite-dir=directory' prior to 'make check'"; \
        exit 1; \
    fi
	@if test `$(cpp_call) -v /dev/null 2>&1 | grep 'MCPP' > /dev/null; echo $$?` != 0; \
            then \
        echo "ERROR: Do 'make install' prior to 'make check'"; \
        exit 1; \
    fi
	@if test -f "$(gcc_testsuite_dir)/gcc.dg/cpp-test/test-t/cpp-test.exp"; \
            then :; \
    else \
        echo '  cp -Rp $(top_srcdir)/cpp-test $(gcc_testsuite_dir)/gcc.dg'; \
        cp -Rp $(top_srcdir)/cpp-test "$(gcc_testsuite_dir)/gcc.dg"; \
    fi
	@$(top_srcdir)/tests/set_test.sh '$(CC)' '$(gcc_path)' '$(gcc_testsuite_dir)' $(gcc_maj_ver) '$(LN_S)' '$(cpp_call)' '$(target_cc)'
	-$(RUNTEST) --tool gcc --srcdir $(gcc_testsuite_dir) cpp-test.exp
	@$(top_srcdir)/tests/after_test.sh '$(CC)' '$(gcc_path)' '$(cpp_call)' '$(target_cc)'

uninstall:
	@if test x"$(gcc_testsuite_dir)" != x""; then \
        cd "$(gcc_testsuite_dir)"; \
        if test -f "gcc.dg/cpp-test/test-t/cpp-test.exp"; then \
            echo "  rm -rf gcc.dg/cpp-test"; \
            rm -rf gcc.dg/cpp-test; \
        fi \
    fi

endif
