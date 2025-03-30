CC = gcc

CFLAGS_DEBUG   = -g -Wall -Wextra -pedantic -O0 -std=c18 -DDEBUG

CFLAGS_PROD    = -O3 -march=native -mtune=native \
                 -flto -fno-fat-lto-objects \
                 -funroll-loops -fomit-frame-pointer \
                 -ffast-math -ftree-vectorize \
                 -std=c18 -DNDEBUG \
                 -Wall -Wextra -pedantic

SRCS           = src/main.c
OBJS           = $(addprefix build/, $(notdir $(SRCS:.c=.o))) 
TARGET         = build/norm

all: prod

prod: CFLAGS = $(CFLAGS_PROD)
prod: $(TARGET)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(TARGET)

$(shell mkdir -p build)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

build:
	mkdir -p build

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

run: $(TARGET)
	./$(TARGET)

run-gdb: debug
	gdb --args ./$(TARGET)

clean:
	rm -rf build

.PHONY: all prod debug run clean