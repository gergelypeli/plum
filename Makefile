.PHONY: build clean
SHELL      = /bin/zsh

DECLS      = all declaration identifier scope type basic record reference interface class option allocable function associable float container nosy
VALUES     = all value literal function boolean integer array reference type typedefinition block record multi generic control stream string iterator class queue rbtree rbtree_helpers rbtree_mapset container option equality float nosy
ARCHS      = ork asm64 storage basics
GLOBALS    = all builtins builtins_errno typespec typematch functions runtime modules
MODULES    = tokenize treeize tupleize typize util structs plum $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%) $(GLOBALS:%=globals/%)
HEADERS    = all globals/all declarations/all values/all
ENVHEADERS = heap typedefs text
SOURCES    = $(MODULES:%=%.cpp) $(HEADERS:%=%.h) $(ENVHEADERS:%=environment/%.h)
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -Wno-unused-parameter -Wno-psabi -g -fdiagnostics-color=always

MAIN       = plum.cpp
BIN        = run/plum
BINFLAGS   = 
GCCLOG     = run/gcc.log
CORE       = core.plum.*(N) core.app.*(N)
BINLOG     = run/plum.log

PRECOMPIN  = precompiled.h
PRECOMPOUT = precompiled.h.gch

MAINDEPS   = $(ENVHEADERS:%=environment/%.h)
MAINSRC    = environment/main.c
MAINOBJ    = run/main.o

FPCONVOBJ  = run/fpconv.o
FPCONVSRC  = environment/fpconv/fpconv.c

TESTBIN    = run/app
TESTOBJ    = run/app.o
TESTSRC    = test/app.plum
TESTLIBS   = -lpcre2-16 -lm
TESTINPUT  = test/input.sh
TESTLOG    = run/app.log
TESTLOGOK  = test/app.log.ok

exe: uncore $(BIN)

test: uncore untest $(TESTBIN)
	@$(TESTINPUT) | $(TESTBIN) 2>&1 | tee $(TESTLOG)
	@diff -ua $(TESTLOGOK) $(TESTLOG)

uncore:
	@rm -f $(CORE)

untest:
	@rm -f $(TESTBIN) $(TESTOBJ)

$(BIN): $(SOURCES)
	@clear
	@$(COMPILE) -o $@ $(CFLAGS) $(MAIN) > $(GCCLOG) 2>&1 || { head -n 30 $(GCCLOG); false }

$(PRECOMPOUT): $(PRECOMPIN)
	@$(COMPILE) $(CFLAGS) -o $@ $<

$(TESTBIN): $(MAINOBJ) $(FPCONVOBJ) $(TESTOBJ)
	@gcc $(CFLAGS) -o $(TESTBIN) $(MAINOBJ) $(FPCONVOBJ) $(TESTOBJ) $(TESTLIBS)

$(MAINOBJ): $(MAINSRC) $(MAINDEPS)
	@gcc $(CFLAGS) -c -o $(MAINOBJ) $(MAINSRC)

$(FPCONVOBJ):
	@gcc $(CFLAGS) -c -o $(FPCONVOBJ) $(FPCONVSRC)

$(TESTOBJ): $(TESTSRC) $(BIN)
	@$(BIN) $(BINFLAGS) $(TESTSRC) $(TESTOBJ) > $(BINLOG) 2>&1 || { cat $(BINLOG); false } && { cat $(BINLOG) }

clean:
	@rm -f $(BIN) $(TESTBIN) $(MAINOBJ) $(TESTOBJ) $(PRECOMPOUT)
