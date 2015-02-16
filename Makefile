CC=clang
all:
	$(CC) runtime.c min-rt.c -o min-rt -lm
clean:
	rm -f min-rt
