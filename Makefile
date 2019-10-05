
BIN = cnc
SRCS = \
    errors.cpp cnc.cpp main.cpp keyboard.cpp workflow.cpp \
    geom.cpp gcode.cpp settings.cpp shapes.cpp dispatch.cpp

TESTS = geom_test.cpp gcode_test.cpp height_map_test.cpp workflow_test.cpp dispatch_test.cpp

LIBS = readline

CXX = g++
CXXFLAGS = -std=gnu++17 -Wall -Wextra -Werror -O0 -ggdb


OBJS = $(patsubst %.cpp,.obj/%.cpp.o,$(SRCS))

all: $(BIN)

.obj/$(BIN).a: $(patsubst %,.obj/%.o,$(filter-out main.cpp,$(SRCS)))
	ar cr $@ $^

$(BIN): .obj/$(BIN).a .obj/main.cpp.o Makefile
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ .obj/main.cpp.o .obj/$(BIN).a $(patsubst %,-l%,$(LIBS))

.obj/%.o: % Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ -MD -MF $(patsubst %,.obj/%.d,$(subst /,_,$<)) -MT $@ $<

clean:
	rm -rf $(BIN) .obj


tests/all: $(patsubst %,.obj/tests/%.o,$(TESTS)) .obj/tests/main.cpp.o .obj/$(BIN).a Makefile
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(patsubst %,.obj/tests/%.o,$(TESTS)) .obj/tests/main.cpp.o .obj/$(BIN).a $(patsubst %,-l%,$(LIBS))

test: tests/all
	$<

.PHONY: all test clean

-include .obj/*.d
