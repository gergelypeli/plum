.PHONY: build clean

MODULES    = tokenize treeize tupleize typize util parse arch/ork arch/x64
SOURCES    = $(MODULES:%=%.cpp)
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -g -fdiagnostics-color=always

MAIN       = parse.cpp
EXE        = plum
CORE       = core.$(EXE)


build: $(EXE)

$(EXE): $(SOURCES)
	@clear
	@rm -f $(CORE)
	@$(COMPILE) -o $@ $(CFLAGS) $(MAIN) 2>&1 | head -n 30

clean:
	rm -f $(EXE)
