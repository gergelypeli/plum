.PHONY: build clean
SHELL      = /bin/zsh

DECLS      = declaration identifier scope type basic record reference interface class option allocable function metatype float
VALUES     = value literal function boolean integer array reference type typedefinition block record multi generic control stream string iterator class circularray rbtree rbtree_helpers container option equality float
ARCHS      = ork x64 storage runtime basics
MODULES    = tokenize treeize tupleize typize util plum builtins global_types global_functions global_factories $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%)
HEADERS    = builtins global_types global_functions arch/ork arch/x64 arch/heap
SOURCES    = $(MODULES:%=%.cpp) $(HEADERS:%=%.h)
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -Wno-unused-parameter -g -fdiagnostics-color=always

MAIN       = plum.cpp
EXE        = run/plum
EXEFLAGS   = 
CORE       = core.plum.*(N) core.test.*(N)

HEAPH      = arch/heap.h
RUNTIMESRC = run/runtime.c
RUNTIMEOBJ = run/runtime.o

TEST       = run/test
TESTOBJ    = run/test.o
TESTSRC    = run/test.plum
#TESTLOG    = run/plum.log
TESTLIBS   = -lpcre2-16 -lm

exe: uncore $(EXE)

test: uncore untest $(TEST)

uncore:
	@rm -f $(CORE)

untest:
	@rm -f $(TEST) $(TESTOBJ)

$(EXE): $(SOURCES)
	@clear
	@set -o pipefail; $(COMPILE) -o $@ $(CFLAGS) $(MAIN) 2>&1 | head -n 30

$(TEST): $(RUNTIMEOBJ) $(TESTOBJ)
	@gcc $(CFLAGS) -o $(TEST) $(RUNTIMEOBJ) $(TESTOBJ) $(TESTLIBS)

$(RUNTIMEOBJ): $(RUNTIMESRC) $(HEAPH)
	@gcc $(CFLAGS) -c -o $(RUNTIMEOBJ) $(RUNTIMESRC)

$(TESTOBJ): $(TESTSRC) $(EXE)
	@$(EXE) $(EXEFLAGS) $(TESTSRC) $(TESTOBJ) # 2>&1 | tee $(TESTLOG)

clean:
	@rm -f $(EXE) $(TEST) $(RUNTIMEOBJ) $(TESTOBJ)
