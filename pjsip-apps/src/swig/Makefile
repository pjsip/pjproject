include ../../../build.mak

ifneq ($(findstring android,$(TARGET_NAME)),)
    # no python for android
    LANG = java csharp
else
    ifneq ($(findstring ios,$(TARGET_NAME)),)
  	LANG = csharp
    else
  	LANG = python java
    endif
endif

CMD_is_java := $(findstring java, $(MAKECMDGOALS))
CMD_is_python := $(findstring python, $(MAKECMDGOALS))
CMD_is_csharp := $(findstring csharp, $(MAKECMDGOALS))
CMD_is_lang := $(or $(CMD_is_java), $(CMD_is_python), $(CMD_is_csharp))

.PHONY: all clean dep depend distclean print realclean install uninstall csharp java python

all: symbols.i
	
all clean dep depend distclean print realclean install uninstall: $(LANG)

java python csharp:
ifeq ($(CMD_is_lang),)
	$(MAKE) $(MAKE_FLAGS) --no-print-directory -C $@ $(MAKECMDGOALS)
else
	$(MAKE) $(MAKE_FLAGS) --no-print-directory -C $@
endif

symbols.i: symbols.lst
	@echo warning: file symbols.i is out of date. Run 'make symbol'.

symbol symbols:
	python importsym.py
