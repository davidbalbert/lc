CFLAGS := -g

default: eval

.PHONY: clean
clean:
	rm -rf *.o eval bootstrap test test.c *.dSYM
