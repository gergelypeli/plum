.PHONY: build clean

MODULES    = stage_1_tokenize stage_2_opize stage_3_typize util parse
SOURCES    = $(MODULES:%=%.cpp)
COMPILE    = g++
CFLAGS     = -Wall -Werror -g

EXE        = plum


build: $(EXE)

$(EXE): $(SOURCES)
	$(COMPILE) -o $@ $(CFLAGS) parse.cpp

clean:
	rm -f $(EXE)
