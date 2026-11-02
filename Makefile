CC      := x86_64-w64-mingw32-gcc
CFLAGS  := -Wall -Wextra -O2
TARGET  := ntverpctl.exe
SRCS    := main.c ntverpctl_fs.c ntverpctl_parse.c ntverpctl_commands.c
OBJS    := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c ntverpctl.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
