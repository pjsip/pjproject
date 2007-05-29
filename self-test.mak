#
# This is a make file for performing various tests on the libraries
#
# Sample user.mak contents:
#  export CFLAGS += -Wno-unused-label -Werror
#
#  ifeq ($(CPP_MODE),1)
#  export CFLAGS += -x c++
#  export LDFLAGS += -lstdc++
#  endif

.PHONY: build_test distclean rm_build_mak build_mak everything pjlib_test pjlib_util_test pjnath_test pjsip_test cpp_prep cpp_test cpp_post

build_test: distclean rm_build_mak build_mak everything cpp_prep cpp_test cpp_post everything
 
all: pjlib_test pjlib_util_test pjnath_test pjsip_test 

CPP_DIR=pjlib pjlib-util pjnath pjmedia pjsip


distclean:
	make distclean

rm_build_mak:
	rm -f build.mak

build_mak:
	./configure
	make dep

everything: 
	make

pjlib_test:
	cd pjlib/bin && ./pjlib-test-`../../config.guess`

pjlib_util_test:
	cd pjlib-util/bin && ./pjlib-util-test-`../../config.guess`

pjnath_test:
	cd pjnath/bin && ./pjnath-test-`../../config.guess`

pjsip_test:
	cd pjsip/bin && ./pjsip-test-`../../config.guess`

cpp_prep:
	for dir in $(CPP_DIR); do \
		make -C $$dir/build clean; \
	done

cpp_test:
	make -f c++-build.mak

cpp_post:
	make -f c++-build.mak clean

