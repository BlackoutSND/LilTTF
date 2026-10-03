CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=c99 -I../../include -I../../include/LilTTF
TARGET = test_runner

SRCS = main_test.c \
       ../../src/FileReader.c \
       ../../src/FontParser.c \
       ../../src/Rasterizer.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
