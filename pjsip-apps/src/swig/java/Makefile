include ../../../../build.mak

OUT_DIR=output
JAVA_SO=$(OUT_DIR)/libpjsua2.so

JDK=/usr/lib/jvm/java-6-sun

all: $(JAVA_SO)

$(JAVA_SO): $(OUT_DIR)/pjsua2_wrap.o
	gcc -shared -Wl,-soname,libpjsua2.so $(OUT_DIR)/pjsua2_wrap.o -o $(JAVA_SO) -lstdc++ $(APP_LDFLAGS)

$(OUT_DIR)/pjsua2_wrap.o: $(OUT_DIR)/pjsua2_wrap.cpp Makefile
	gcc -c $(OUT_DIR)/pjsua2_wrap.cpp -fPIC -o $(OUT_DIR)/pjsua2_wrap.o  -I$(JDK)/include -I$(JDK)/include/linux $(APP_CFLAGS)

$(OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i $(SRCS)
	swig $(SWIG_FLAGS) -java  -o $(OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(OUT_DIR)/pjsua2_wrap.cpp $(OUT_DIR)/pjsua2_wrap.o $(JAVA_SO) $(OUT_DIR)/*.java $(OUT_DIR)/*.class
	ln -s ../test.java output/test.java
