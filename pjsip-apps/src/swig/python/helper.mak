include ../../../../build.mak
include $(PJDIR)/build/common.mak

ldflags:
	@for token in `echo $(PJ_LDXXFLAGS) $(LDFLAGS)`; do \
		echo $$token; \
	done
	@for token in `echo $(PJ_LDXXLIBS) $(LIBS)`; do \
		echo $$token | grep -v \\-l; \
	done

libs:
	@for token in `echo $(PJ_LDXXLIBS) $(LIBS)`; do \
		echo $$token | grep \\-l | sed 's/-l//'; \
	done

cflags:
	@for token in `echo $(PJ_CXXFLAGS) $(CFLAGS)`; do \
		echo $$token; \
	done

target_name:
	@echo $(TARGET_NAME)
	
