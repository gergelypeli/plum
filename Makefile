.PHONY: build clean

MODULES    = stage_1_tokenize stage_2_treeize stage_3_tupleize stage_4_typize util parse
SOURCES    = $(MODULES:%=%.cpp)
MAIN       = parse.cpp
COMPILE    = g++
CFLAGS     = -Wall -Werror -g -fdiagnostics-color=always

EXE        = plum
CORE       = core.$(EXE)


build: $(EXE)

$(EXE): $(SOURCES)
	@clear
	@rm -f $(CORE)
	@$(COMPILE) -o $@ $(CFLAGS) $(MAIN) 2>&1 | head -n 30

clean:
	rm -f $(EXE)
