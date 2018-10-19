
BIN = cnc
SRCS = errors.cpp cnc.cpp main.cpp

LIBS = readline

CXX = g++
CXXFLAGS = -std=gnu++14 -Wall -Wextra -Werror -O0 -ggdb


OBJS = $(patsubst %.cpp,.obj/%.cpp.o,$(SRCS))

all: $(BIN)

$(BIN): $(OBJS) Makefile
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(patsubst %,-l%,$(LIBS))

.obj/%.o: % Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ -MD -MF $(patsubst %,.obj/%.d,$<) -MT $@ $<

clean:
	rm -rf $(BIN) .obj


-include .obj/*.d
