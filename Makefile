CC      ?= cc
PREFIX  ?= /usr/local
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wshadow -Wpointer-arith -I.
LDFLAGS ?=
LDLIBS  ?=

SRCDIR  := src
BUILD   := build
BIN     := $(BUILD)/pls
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

.PHONY: all clean install uninstall test

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: $(SRCDIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

test: $(BIN)
	@./tests/run.sh

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/pls

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/pls

clean:
	rm -rf $(BUILD)

-include $(DEPS)
