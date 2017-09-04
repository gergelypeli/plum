.PHONY: build clean
SHELL      = /bin/zsh

DECLS      = declaration identifier scope type typespec basic record reference
VALUES     = value literal function boolean integer array reference type block record multi generic control
ARCHS      = ork x64 storage
MODULES    = tokenize treeize tupleize typize util plum $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%)
SOURCES    = $(MODULES:%=%.cpp) arch/ork.h arch/x64.h arch/heap.h
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -Wno-unused-parameter -g -fdiagnostics-color=always

TOP        = plum.cpp
EXE        = run/plum
CORE       = core.plum.*(N) core.test.*(N)

MAIN       = run/main

TEST       = run/test
TESTSOURCE = run/test.c
TESTOBJECT = run/test.o
TESTMODULE = run/mymodule.o
TESTPLUM   = run/first.plum

exe: $(EXE)

test: $(TEST)

$(EXE): $(SOURCES)
	@clear
	@set -o pipefail; $(COMPILE) -o $@ $(CFLAGS) $(TOP) 2>&1 | head -n 30

$(TEST): $(TESTOBJECT) $(TESTMODULE)
	@rm -f $(CORE)
	@gcc $(CFLAGS) -o $(TEST) $(TESTOBJECT) $(TESTMODULE)

$(TESTOBJECT): $(TESTSOURCE)
	@gcc $(CFLAGS) -c -o $(TESTOBJECT) $(TESTSOURCE)

$(TESTMODULE): $(TESTPLUM) $(EXE)
	@$(EXE) $(TESTPLUM) $(TESTMODULE)

clean:
	@rm -f $(EXE) $(TEST) $(TESTOBJECT) $(TESTMODULE)
