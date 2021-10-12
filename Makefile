CFLAGS := -g

default: bootstrap

bootstrap: bootstrap.c runtime.c

.PHONY: clean
clean:
	rm -rf *.o eval bootstrap test test.c *.dSYM
