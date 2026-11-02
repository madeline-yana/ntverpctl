CC      := x86_64-w64-mingw32-gcc
CFLAGS  := -Wall -Wextra -O2
TARGET  := razzle.exe
SRCS    := main.c razzle_fs.c razzle_parse.c razzle_commands.c
OBJS    := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c razzle.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
