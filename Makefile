CFLAGS := -g

default: bootstrap

.PHONY: clean
clean:
	rm -rf *.o eval bootstrap test test.c *.dSYM
