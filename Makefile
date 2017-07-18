.PHONY: build clean

DECLS      = declaration identifier scope type
VALUES     = value literal function boolean integer
ARCHS      = ork x64
MODULES    = tokenize treeize tupleize typize util plum $(DECLS:%=declarations/%) $(VALUES:%=values/%) $(ARCHS:%=arch/%)
SOURCES    = $(MODULES:%=%.cpp)
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -g -fdiagnostics-color=always

TOP        = plum.cpp
EXE        = run/plum
CORE       = core.plum

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
	@rm -f $(CORE)
	@$(COMPILE) -o $@ $(CFLAGS) $(TOP) 2>&1 | head -n 30

$(TEST): $(TESTOBJECT) $(TESTMODULE)
	@gcc $(CFLAGS) -o $(TEST) $(TESTOBJECT) $(TESTMODULE)
	
$(TESTOBJECT): $(TESTSOURCE)
	@gcc $(CFLAGS) -c -o $(TESTOBJECT) $(TESTSOURCE)

$(TESTMODULE): $(TESTPLUM) $(EXE)
	@$(EXE) $(TESTPLUM) $(TESTMODULE)

clean:
	@rm -f $(EXE) $(TEST) $(TESTOBJECT) $(TESTMODULE)
