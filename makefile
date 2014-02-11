CFLAGS += -static -s -Os

all: textout

textout: textout.c

clean:
	rm -f textout textout.o
