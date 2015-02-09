all:
	clang -O0 -g3 runtime.c min-rt.c -o min-rt -lm
clean:
	rm -f min-rt
