include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export SRTP_LIB := libsrtp-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export SRTP_SONAME := libsrtp.$(SHLIB_SUFFIX)
export SRTP_SHLIB := $(SRTP_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_INC). $(CC_INC)../../srtp/crypto/include \
		   $(CC_INC)../../srtp/include \
		   $(CC_INC)../../../pjlib/include \
		   $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) 
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(APP_LDFLAGS) $(LDFLAGS) 

# libcrypt.a (the crypto engine) 
ciphers = crypto/cipher/cipher.o crypto/cipher/null_cipher.o      \
          crypto/cipher/aes.o crypto/cipher/aes_icm.o             \
          crypto/cipher/aes_cbc.o

hashes  = crypto/hash/null_auth.o crypto/hash/sha1.o \
          crypto/hash/hmac.o crypto/hash/auth.o # crypto/hash/tmmhv2.o 

replay  = crypto/replay/rdb.o crypto/replay/rdbx.o               \
          crypto/replay/ut_sim.o 

math    = crypto/math/datatypes.o crypto/math/stat.o

ust     = crypto/ust/ust.o 

rng     = crypto/rng/rand_source.o crypto/rng/prng.o crypto/rng/ctr_prng.o

err     = pjlib/srtp_err.o

kernel  = crypto/kernel/crypto_kernel.o  crypto/kernel/alloc.o   \
          crypto/kernel/key.o $(rng) $(err) # $(ust) 

srtpobj = srtp/srtp.o 

cryptobj =  $(ciphers) $(hashes) $(math) $(stat) $(kernel) $(replay)

export SRTP_SRCDIR = ../../srtp
export SRTP_OBJS = $(cryptobj) $(srtpobj)
export SRTP_CFLAGS = -DHAVE_CONFIG_H $(_CFLAGS)
export SRTP_LDFLAGS = $(PJLIB_LDLIB) $(_LDFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(SRTP_LIB) $(SRTP_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(SRTP_LIB) $(SRTP_SONAME)

libsrtp: $(SRTP_LIB)
$(SRTP_SONAME): $(SRTP_LIB)
$(SRTP_LIB) $(SRTP_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=SRTP app=libsrtp $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=SRTP app=libsrtp $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=SRTP app=libsrtp $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=SRTP app=libsrtp $@



