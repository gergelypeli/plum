.PHONY: build clean
SHELL      = /bin/zsh

DECLS      = declaration identifier scope type typespec basic record reference interface class
VALUES     = value literal function boolean integer array reference type block record multi generic control stream iterator class circularray rbtree container
ARCHS      = ork x64 storage
MODULES    = tokenize treeize tupleize typize util plum builtin once unwind $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%)
SOURCES    = $(MODULES:%=%.cpp) builtin.h arch/ork.h arch/x64.h arch/heap.h
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -Wno-unused-parameter -g -fdiagnostics-color=always

TOP        = plum.cpp
EXE        = run/plum
CORE       = core.plum.*(N) core.test.*(N)

MAIN       = run/main

HEAPH      = arch/heap.h
RUNTIMESRC = run/runtime.c
RUNTIMEOBJ = run/runtime.o

TEST       = run/test
TESTOBJ    = run/test.o
TESTSRC    = run/test.plum
#TESTLOG    = run/plum.log

exe: uncore $(EXE)

test: uncore $(TEST)

uncore:
	@rm -f $(CORE)

$(EXE): $(SOURCES)
	@clear
	@set -o pipefail; $(COMPILE) -o $@ $(CFLAGS) $(TOP) 2>&1 | head -n 30

$(TEST): $(RUNTIMEOBJ) $(TESTOBJ)
	@gcc $(CFLAGS) -o $(TEST) $(RUNTIMEOBJ) $(TESTOBJ)

$(RUNTIMEOBJ): $(RUNTIMESRC) $(HEAPH)
	@gcc $(CFLAGS) -c -o $(RUNTIMEOBJ) $(RUNTIMESRC)

$(TESTOBJ): $(TESTSRC) $(EXE)
	@$(EXE) $(TESTSRC) $(TESTOBJ) # 2>&1 | tee $(TESTLOG)

clean:
	@rm -f $(EXE) $(TEST) $(RUNTIMEOBJ) $(TESTOBJ)
