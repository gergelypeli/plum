.PHONY: build clean
SHELL      = /bin/zsh

DECLS      = declaration identifier scope type typespec basic record reference interface class
VALUES     = value literal function boolean integer array reference type block record multi generic control stream iterator stack class
ARCHS      = ork x64 storage
MODULES    = tokenize treeize tupleize typize util plum builtin $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%)
SOURCES    = $(MODULES:%=%.cpp) builtin.h arch/ork.h arch/x64.h arch/heap.h
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
TESTLOG    = run/plum.log

exe: uncore $(EXE)

test: uncore $(TEST)

uncore:
	@rm -f $(CORE)

$(EXE): $(SOURCES)
	@clear
	@set -o pipefail; $(COMPILE) -o $@ $(CFLAGS) $(TOP) 2>&1 | head -n 30

$(TEST): $(TESTOBJECT) $(TESTMODULE)
	@gcc $(CFLAGS) -o $(TEST) $(TESTOBJECT) $(TESTMODULE)

$(TESTOBJECT): $(TESTSOURCE)
	@gcc $(CFLAGS) -c -o $(TESTOBJECT) $(TESTSOURCE)

$(TESTMODULE): $(TESTPLUM) $(EXE)
	@$(EXE) $(TESTPLUM) $(TESTMODULE) # 2>&1 | tee $(TESTLOG)

clean:
	@rm -f $(EXE) $(TEST) $(TESTOBJECT) $(TESTMODULE)
