.PHONY: build clean
SHELL      = /bin/zsh

DECLS      = all declaration identifier scope type basic record reference interface class singleton option allocable function metatype float
VALUES     = all value literal function boolean integer array reference type typedefinition block record multi generic control stream string iterator class circularray rbtree rbtree_helpers container option equality float
ARCHS      = ork asm64 storage basics
GLOBALS    = all builtins builtins_errno typespec typematch functions runtime modules
MODULES    = tokenize treeize tupleize typize util structs plum $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%) $(GLOBALS:%=globals/%)
HEADERS    = all globals/all declarations/all values/all environment/heap environment/typedefs environment/utf8
SOURCES    = $(MODULES:%=%.cpp) $(HEADERS:%=%.h)
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -Wno-unused-parameter -Wno-psabi -g -fdiagnostics-color=always

MAIN       = plum.cpp
EXE        = run/plum
#EXEFLAGS   = -m
CORE       = core.plum.*(N) core.test.*(N)

PRECOMPIN  = precompiled.h
PRECOMPOUT = precompiled.h.gch

HEAPH      = environment/heap.h
MAINSRC    = environment/main.c
MAINOBJ    = run/main.o

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

$(PRECOMPOUT): $(PRECOMPIN)
	@$(COMPILE) $(CFLAGS) -o $@ $<

$(TEST): $(MAINOBJ) $(TESTOBJ)
	@gcc $(CFLAGS) -o $(TEST) $(MAINOBJ) $(TESTOBJ) $(TESTLIBS)

$(MAINOBJ): $(MAINSRC) $(HEAPH)
	@gcc $(CFLAGS) -c -o $(MAINOBJ) $(MAINSRC)

$(TESTOBJ): $(TESTSRC) $(EXE)
	@$(EXE) $(EXEFLAGS) $(TESTSRC) $(TESTOBJ) # 2>&1 | tee $(TESTLOG)

clean:
	@rm -f $(EXE) $(TEST) $(MAINOBJ) $(TESTOBJ) $(PRECOMPOUT)
