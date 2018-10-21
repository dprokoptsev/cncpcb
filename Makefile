
BIN = cnc
SRCS = errors.cpp cnc.cpp main.cpp keyboard.cpp workflow.cpp

TESTS = geom_test.cpp

LIBS = readline

CXX = g++
CXXFLAGS = -std=gnu++14 -Wall -Wextra -Werror -O0 -ggdb


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


define unit_test
all: tests/$(1)
tests/$(1): .obj/tests/$(1).cpp.o .obj/tests/main.cpp.o .obj/$(BIN).a Makefile
	$$(CXX) $$(CXXFLAGS) $$(LDFLAGS) -o $$@ $$< .obj/tests/main.cpp.o .obj/$$(BIN).a $$(patsubst %,-l%,$$(LIBS))
run_$(1): tests/$(1)
	$$<
test: run_$(1)
.PHONY: run_$(1)
endef


$(foreach t,$(TESTS),$(eval $(call unit_test,$(patsubst %.cpp,%,$(t)))))

.PHONY: all test clean

-include .obj/*.d
