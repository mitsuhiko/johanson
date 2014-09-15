CONFIGS=release debug

all: compile compile-debug

solutions/Makefile: premake4.lua
	premake4 --to=solutions gmake

solutions/vs2010:
	premake4 --to=solutions/vs2010 vs2010

solutions: solutions/Makefile solutions/vs2010

compile: solutions/Makefile
	@$(MAKE) -C solutions

compile-debug: solutions/Makefile
	@$(MAKE) -C solutions config=debug

compile-all: solutions/Makefile
	@for cfg in ${CONFIGS}; do $(MAKE) -C solutions config="$${cfg}"; done

clean:
	@rm -rf solutions
	@rm -rf build

test: compile-all
	@$(MAKE) -C tests test

.PHONY: all solutions compile compile-debug compile-all clean
