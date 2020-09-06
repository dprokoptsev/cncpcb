
BIN = cnc
SRCS = \
    errors.cpp cnc.cpp main.cpp keyboard.cpp workflow.cpp \
    geom.cpp gcode.cpp settings.cpp shapes.cpp dispatch.cpp

TESTS = \
	geom_test.cpp gcode_test.cpp height_map_test.cpp workflow_test.cpp \
	dispatch_test.cpp shapes_test.cpp

LIBS = readline tinfo

ARCH = amd64 armhf

CXX_amd64 = g++
CXX_armhf = /opt/cross-pi-gcc/bin/arm-linux-gnueabihf-g++
CXXFLAGS = -std=gnu++17 -Wall -Wextra -Werror -Wno-psabi -O0 -ggdb

define make_arch
# $(1) - arch

OBJS_$(1) = $$(patsubst %.cpp,.obj/$(1)/%.cpp.o,$(filter-out main.cpp,$(SRCS)))

all: all@$(1)

all@$(1): .obj/$(1)/$(BIN)

.obj/$(1)/$(BIN): .obj/$(1)/$(BIN).a .obj/$(1)/main.cpp.o Makefile
	$$(CXX_$(1)) $(CXXFLAGS) $$(CXXFLAGS_$(1)) $(LDFLAGS) $$(LDFLAGS_$(1)) -o $$@ \
	    .obj/$(1)/main.cpp.o .obj/$(1)/$(BIN).a $(patsubst %,-l%,$(LIBS))

.obj/$(1)/$(BIN).a: $$(OBJS_$(1))
	ar cr $$@ $$^

.obj/$(1)/%.cpp.o: %.cpp Makefile
	@mkdir -p $$(dir $$@) .obj/deps
	$$(CXX_$(1)) $(CXXFLAGS) $$(CXXFLAGS_$(1)) -c -o $$@ \
	    -MD -MF $$(patsubst %,.obj/deps/$(1)_%.d,$$(subst /,_,$$<)) -MT $$@ $$<

clean-$(1):
	rm -rf .obj/$(1)

clean: clean-$(1)
.PHONY: clean-$(1)

endef

$(foreach arch,$(ARCH),$(eval $(call make_arch,$(arch))))

HOST_ARCH = amd64

tests/all: $(patsubst %,.obj/$(HOST_ARCH)/tests/%.o,$(TESTS)) .obj/$(HOST_ARCH)/tests/main.cpp.o .obj/$(HOST_ARCH)/$(BIN).a Makefile
	$(CXX_$(HOST_ARCH)) $(CXXFLAGS) $(CXXFLAGS_$(HOST_ARCH)) $(LDFLAGS) $(LDFLAGS_$(TESTARCH)) \
	    -o $@ $(patsubst %,.obj/$(HOST_ARCH)/tests/%.o,$(TESTS)) \
	    .obj/$(HOST_ARCH)/tests/main.cpp.o .obj/$(HOST_ARCH)/$(BIN).a $(patsubst %,-l%,$(LIBS))

test: tests/all
	$<

deploy: .obj/armhf/$(BIN) $(SRCS)
	tar -czf- $^ | ssh cnc "\
	    mkdir -p src/cncpcb \
	    && tar -C src/cncpcb -xzf- \
	    && mv src/cncpcb/.obj/armhf/$(BIN) ~/bin/cnc"


all: $(BIN)

$(BIN): .obj/$(HOST_ARCH)/$(BIN)
	ln -sf $< $@

.PHONY: all test clean deploy

-include .obj/deps/*.d
