CC=clang
CFLAGS= -g -O2 -ansi -pedantic-errors -Wno-comment
all:
	$(CC) $(CFLAGS) min-rt.c -o min-rt -lm
clean:
	rm -f min-rt
