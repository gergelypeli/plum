.PHONY: build clean

DECLS      = declaration identifier scope type basic record reference interface class option allocable function associable float container nosy util
VALUES     = value literal function boolean integer array reference type typedefinition block record multi generic control stream string iterator class queue rbtree rbtree_helpers rbtree_mapset rbtree_weakmapset container option equality float weakref debug
ARCHS      = elf elf_x64 elf_a64 asm asm_x64 asm_a64 emu emu_x64 emu_a64 storage basics dwarf
PARSING    = tokenize treeize tupleize typize
GLOBALS    = builtins builtins_errno typespec typematch runtime modules
MODULES    = util plum $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%) $(GLOBALS:%=globals/%) $(PARSING:%=parsing/%)
OBJECTS    = $(MODULES:%=build/%.o)

ALLHEADERS = parsing/all globals/all declarations/all values/all
ENVHEADERS = shared
HEADERS    = $(MODULES:%=%.h) $(ALLHEADERS:%=%.h) $(ENVHEADERS:%=environment/%.h)

PRECOMP    = plum.h.gch

GCC        = gcc
GXX        = g++
CFLAGS     = -Wall -Wextra -Werror -Wno-unused-parameter -Wno-psabi -g -fdiagnostics-color=always

BIN        = run/plum
BINFLAGS   = 
GCCLOG     = run/gcc.log
BINLOG     = run/plum.log

MAINDEPS   = $(ENVHEADERS:%=environment/%.h)
MAINSRC    = environment/main.c
MAINOBJ    = build/environment/main.o
SHAREDSRC  = environment/shared.c
SHAREDOBJ  = build/environment/shared.o

FPCONVOBJ  = build/environment/fpconv/fpconv.o
FPCONVSRC  = environment/fpconv/fpconv.c

TESTBIN    = run/app
TESTOBJ    = build/test/app.o
TESTSRC    = test/app.plum
TESTLIBS   = -lpcre2-16 -lm
TESTINPUT  = test/input.sh
TESTLOG    = run/app.log
TESTLOGOK  = test/app.log.ok

# General

exe: uncore $(BIN)

test: uncore $(TESTBIN)
	@$(TESTINPUT) | $(TESTBIN) 2>&1 | tee $(TESTLOG)
	@diff -ua $(TESTLOGOK) $(TESTLOG)

testobj: $(TESTOBJ)

uncore:
	@for x in core*; do if file -b $$x | grep -q 'ELF 64-bit LSB core file'; then rm $$x; fi; done

clean:
	@rm -f $(BIN) $(TESTBIN) $(OBJECTS) $(MAINOBJ) $(SHAREDOBJ) $(TESTOBJ) $(FPCONVOBJ) $(PRECOMP) run/*.log

# Compiler

$(PRECOMP): plum.h $(HEADERS)
	@echo Precompiling $@
	@$(GXX) $(CFLAGS) -o $@ $<

$(OBJECTS): build/%.o: %.cpp $(PRECOMP)
	@echo Compiling $@
	@mkdir -p $(dir $@)
	@$(GXX) $(CFLAGS) -c -o $@  $<

$(SHAREDOBJ): $(SHAREDSRC)
	@echo Compiling $@
	@mkdir -p $(dir $@)
	@$(GCC) $(CFLAGS) -c -o $@ $<

$(BIN): $(OBJECTS) $(SHAREDOBJ)
	@echo Linking $@
	@$(GXX) $(CFLAGS) -o $@ $^

# Runtime

$(MAINOBJ): $(MAINSRC) $(MAINDEPS)
	@echo Compiling $@
	@mkdir -p $(dir $@)
	@$(GCC) $(CFLAGS) -c -o $@ $<

$(FPCONVOBJ): $(FPCONVSRC)
	@echo Compiling $@
	@mkdir -p $(dir $@)
	@$(GCC) $(CFLAGS) -c -o $@ $<

$(TESTOBJ): $(TESTSRC) $(BIN)
	@echo Compiling test $@
	@mkdir -p $(dir $@)
	@$(BIN) $(BINFLAGS) $< $@ > $(BINLOG) 2>&1 || { cat $(BINLOG); false; } && { cat $(BINLOG); }

$(TESTBIN): $(MAINOBJ) $(FPCONVOBJ) $(SHAREDOBJ) $(TESTOBJ)
	@echo Linking test $@
	@mkdir -p $(dir $@)
	@$(GCC) $(CFLAGS) -o $@ $^ $(TESTLIBS)
