.PHONY: build clean
SHELL      = /bin/zsh

DECLS      = declaration identifier scope type basic record reference interface class option allocable function metatype float
VALUES     = value literal function boolean integer array reference type typedefinition block record multi generic control stream string iterator class circularray rbtree container option equality
ARCHS      = ork x64 storage runtime
MODULES    = tokenize treeize tupleize typize util plum builtins global_types global_functions $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%)
HEADERS    = builtins global_types global_functions arch/ork arch/x64 arch/heap
SOURCES    = $(MODULES:%=%.cpp) $(HEADERS:%=%.h)
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
TESTLIBS   = -lpcre2-16

exe: uncore $(EXE)

test: uncore $(TEST)

uncore:
	@rm -f $(CORE)

$(EXE): $(SOURCES)
	@clear
	@set -o pipefail; $(COMPILE) -o $@ $(CFLAGS) $(TOP) 2>&1 | head -n 30

$(TEST): $(RUNTIMEOBJ) $(TESTOBJ)
	@gcc $(CFLAGS) -o $(TEST) $(RUNTIMEOBJ) $(TESTOBJ) $(TESTLIBS)

$(RUNTIMEOBJ): $(RUNTIMESRC) $(HEAPH)
	@gcc $(CFLAGS) -c -o $(RUNTIMEOBJ) $(RUNTIMESRC)

$(TESTOBJ): $(TESTSRC) $(EXE)
	@$(EXE) $(TESTSRC) $(TESTOBJ) # 2>&1 | tee $(TESTLOG)

clean:
	@rm -f $(EXE) $(TEST) $(RUNTIMEOBJ) $(TESTOBJ)
