CC=clang
CFLAGS= -g -O0 -ansi -pedantic-errors -Wno-comment
all: conv min-rt

conv: conv.c
	$(CC) conv.c -o conv

min-rt: min-rt.c
	$(CC) $(CFLAGS) min-rt.c -o min-rt -lm

clean:
	rm -f min-rt conv
