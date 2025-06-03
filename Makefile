all: build

CC := clang
BIN := drsb
CFLAGS := -std=c99 -Wall -Wextra -Werror -pedantic

build: main.c
	$(CC) $(TARGET) $(CFLAGS) main.c -o $(BIN)

dev: main.c
	$(CC) $(TARGET) $(CFLAGS) -g main.c -o $(BIN)

clean:
	rm $(BIN)
