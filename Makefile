include mk/begin.mk

CCFLAGS += -g -Wall -Wextra -fsanitize=address,undefined

SRC := \
test.cpp \
jjring.cpp \
jjring.test.cpp \
jjmath.test.cpp \
jjrecord.test.cpp \
jjreg.test.cpp \

OBJ := $(addprefix $(OBJDIR)/src/,$(addsuffix .o,$(SRC)))

EXE := $(BUILDDIR)/test
$(EXE): $(OBJ)
	$(MkTargetDir)
	$(LINK.cc) $+ -o "$@"

all: $(EXE)
run: $(EXE)
	./$<

include mk/end.mk
