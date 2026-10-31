all: toyforth

.PHONY: all clean test

toyforth: toyforth.c
	$(CC) toyforth.c -Wall -W -O2 -o toyforth

test: toyforth
	python3 -B -m unittest discover -s tests

clean:
	rm -rf toyforth
