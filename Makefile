.PHONY: build clean

MODULES    = tokenize treeize tupleize typize declarations values util parse arch/ork arch/x64
SOURCES    = $(MODULES:%=%.cpp)
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -g -fdiagnostics-color=always

TOP        = parse.cpp
EXE        = run/plum
CORE       = run/core.plum

MAIN       = run/main

exe: $(EXE)

main: $(MAIN)

$(EXE): $(SOURCES)
	@clear
	@rm -f $(CORE)
	@$(COMPILE) -o $@ $(CFLAGS) $(TOP) 2>&1 | head -n 30

$(MAIN): run/main.o run/mymodule.o
	@gcc -o $(MAIN) run/main.o run/mymodule.o
	
run/main.o: run/main.c
	@gcc -c -o run/main.o run/main.c

run/mymodule.o: run/first.plum $(EXE)
	@cd run && ./plum first.plum

clean:
	@rm -f $(EXE) $(MAIN) run/main.o run/mymodule.o
