all: toyforth

.PHONY: all clean profile

toyforth: toyforth.c
	$(CC) toyforth.c -Wall -W -O2 -o toyforth

profile: toyforth
	./toyforth --profile examples/pi.toy >profiling/pi.stdout.txt 2>profiling/pi.profile.txt
	./toyforth --profile examples/pi_golf.toy >profiling/pi_golf.stdout.txt 2>profiling/pi_golf.profile.txt
	python3 -B profiling/profile_pi.py

test: toyforth
	./tests/test_pi.sh

clean:
	rm -rf toyforth
