all: test

test: tests
	./run_tests.sh ./tests-debug
	./run_tests.sh ./tests-release

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
