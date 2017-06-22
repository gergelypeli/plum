.PHONY: build clean

MAIN       = parse.cpp arch/ia32.cpp arch/ork.cpp
MODULES    = stage_1_tokenize stage_2_treeize stage_3_tupleize stage_4_typize util
SOURCES    = $(MODULES:%=%.cpp) $(MAIN)
COMPILE    = g++
CFLAGS     = -Wall -Wextra -Werror -g -fdiagnostics-color=always

EXE        = plum
CORE       = core.$(EXE)


build: $(EXE)

$(EXE): $(SOURCES)
	@clear
	@rm -f $(CORE)
	@$(COMPILE) -o $@ $(CFLAGS) $(MAIN) 2>&1 | head -n 30

clean:
	rm -f $(EXE)
