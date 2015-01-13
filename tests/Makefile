UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	EXPORTS += LD_LIBRARY_PATH=../build/debug/native:../build/release/native
endif

all: test

test: tests
	$(EXPORTS) ./run_parsing_tests.sh ./parsing-tests-debug
	$(EXPORTS) ./run_parsing_tests.sh ./parsing-tests-release

solutions/Makefile:
	premake4 gmake

tests: solutions/Makefile
	@$(MAKE) -C solutions config=debug
	@$(MAKE) -C solutions config=release

clean:
	@rm -rf solutions
	@rm -rf obj
	@rm -f parsing-tests-debug parsing-tests-release
	@rm -f parsing-cases/*.out
	@rm -f parsing-cases/*.test

.venv:
	virtualenv .venv
	.venv/bin/pip install cffi pytest

api-test: .venv
	.venv/bin/py.test --verbose --tb=short api-tests

.PHONY: tests test clean all
