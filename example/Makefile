all: example

solutions/Makefile:
	premake4 gmake

example: solutions/Makefile
	@$(MAKE) -C solutions

clean:
	@rm -rf solutions
	@rm -rf obj
	@rm -f example
