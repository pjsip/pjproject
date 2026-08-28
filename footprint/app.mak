# Link footprint_app.c against a configured+built pjproject tree.
#
# Usage:
#   make -f app.mak PJDIR=/path/to/pjproject SRC=/path/to/footprint_app.c \
#        OUT=/path/to/output-binary
#
# PJ_CC/PJ_CFLAGS/PJ_LDFLAGS/PJ_LDLIBS come from the tree's generated
# build.mak, so the app is compiled and linked with exactly the same
# toolchain and flags as the library build.

include $(PJDIR)/build.mak

all:
	$(PJ_CC) -o $(OUT) $(SRC) $(PJ_CFLAGS) $(PJ_LDFLAGS) $(PJ_LDLIBS) $(EXTRA_LDFLAGS)
