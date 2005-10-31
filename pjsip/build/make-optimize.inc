
#
# If MINSIZE is defined, optimize for code size.
#
ifdef MINSIZE
PJSIP_OPTIMIZE := $(PJSIP_OPTIMIZE) -DPJSIP_HAS_DUMP=0
else
PJSIP_OPTIMIZE := $(PJSIP_OPTIMIZE)
endif

