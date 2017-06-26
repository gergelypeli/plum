.PHONY: build clean

MODULES    = tokenize treeize tupleize typize typize_declarations typize_values typize_values_integer util plum arch/ork arch/x64
SOURCES    = $(MODULES:%=%.cpp)
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -g -fdiagnostics-color=always

TOP        = plum.cpp
EXE        = run/plum
CORE       = run/core.plum

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
	@gcc -o $(TEST) $(TESTOBJECT) $(TESTMODULE)
	
$(TESTOBJECT): $(TESTSOURCE)
	@gcc -c -o $(TESTOBJECT) $(TESTSOURCE)

$(TESTMODULE): $(TESTPLUM) $(EXE)
	@$(EXE) $(TESTPLUM) $(TESTMODULE)

clean:
	@rm -f $(EXE) $(TEST) $(TESTOBJECT) $(TESTMODULE)
