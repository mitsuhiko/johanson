UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	EXPORTS += "LD_LIBRARY_PATH=../build/debug/native:../build/release/native"
endif

all: test

test: tests
	$(EXPORTS)./run_tests.sh ./tests-debug
	$(EXPORTS) ./run_tests.sh ./tests-release

solutions/Makefile:
	premake4 gmake

tests: solutions/Makefile
	@$(MAKE) -C solutions config=debug
	@$(MAKE) -C solutions config=release

clean:
	@rm -rf solutions
	@rm -rf obj
	@rm -f tests-debug tests-release
	@rm -f cases/*.out
	@rm -f cases/*.test

.PHONY: tests test clean all
