# jjkit

This is my collection of utility libraries and tools, mainly for C++ development.

It is intended to be used by copying the relevant individual parts into your own project and adapting them as necessary.

## `jjring`: a lock-free SPSC queue

`jjring` is a lock-free single-producer, single-consumer (SPSC) queue implementation. It is designed for high-performance scenarios where one thread produces data and another consumes it. The queue uses a circular buffer internally and avoids locks by relying on atomic operations, making it suitable for real-time or low-latency applications.

### Usage
To use `jjring`, include the header and instantiate the queue with the desired data type and buffer size. Example:

```cpp
#include "jjring.hpp"
// ...
jjring<uint8_t, 256> rx_buffer;
// In ISR:
rx_buffer.push(UART->DR);
// In main loop:
uint8_t byte;
if(rx_buffer.pop(byte)) {
	process(byte);
}
```

## `mk`: a make-based build system

A tiny, portable `make` setup. Copy `mk/begin.mk` and `mk/end.mk`, then include them in your `Makefile`.
It provides core rules for C/C++ builds, dependency tracking, and a standard directory layout for outputs.

A minimal `Makefile` looks like this:

```makefile
include mk/begin.mk

SRC := main.cpp
OBJ := $(addprefix $(OBJDIR)/,$(addsuffix .o,$(SRC)))

EXE := $(BUILDDIR)/my_app
$(EXE): $(OBJ)
	$(MkTargetDir)
	$(LINK.cc) $^ -o "$@"

all: $(EXE)
run: $(EXE); ./$<

include mk/end.mk
```

Final built targets are to be placed in `build/<product>/`, and intermediate files in `build/<product>.intermediate/`.
The product name is inferred from the directory name.

Use `make clean` to remove the current product's build files, or `make distclean` to remove the entire `build/` directory.

### Multi-product builds

Create one `Makefile.<product>.mk` for each different product.
Then, use the following `Makefile`:

```make
MAKEFILES := $(wildcard Makefile.*.mk)
TARGETS := $(patsubst Makefile.%.mk,%,$(MAKEFILES))

.PHONY: all $(TARGETS)
all: $(TARGETS)
$(TARGETS): %: Makefile.%.mk
	$(MAKE) -f $< $(filter-out $@,$(MAKECMDGOALS))
```

This allows you to write `make <product0> [...] <productN>` instead of chaining `make -f Makefile.<product>.mk` commands.
All variables and flags will be inherited by the sub-make processes.

# Using jjkit

## Running the test suite

To run the test suite, use the following command:

```bash
make run
```

## Compatibility

This library is designed to be compatible with GCC and Clang using C++14 or later.

## License

All code and content in this repository (excluding `ext/`) is licensed under the MIT License, unless mentioned otherwise.
See [LICENSE](./LICENSE) for more information.
