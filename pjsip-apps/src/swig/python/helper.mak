include ../../../../build.mak
include $(PJDIR)/build/common.mak

ldflags:
	@for token in `echo $(PJ_LDXXLIBS) $(PJ_LDXXFLAGS) $(LDFLAGS)`; do \
		echo $$token; \
	done

cflags:
	@for token in `echo $(PJ_CXXFLAGS) $(CFLAGS)`; do \
		echo $$token; \
	done

target_name:
	@echo $(TARGET_NAME)
	
