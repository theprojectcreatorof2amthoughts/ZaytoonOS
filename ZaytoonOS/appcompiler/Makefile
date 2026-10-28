CC = gcc
CFLAGS = -std=gnu11 -O2 -Wall -Wextra

all: commandline.capp number_guess.capp zayflight.capp

# 1. Compile the host tool executable using standard hosted settings
commandline.builder: commandline.c lib.c lib.h
	$(CC) $(CFLAGS) commandline.c lib.c -o $@

# 2. Run the host tool executable to output the actual commandline.capp file
commandline.capp: commandline.builder
	./commandline.builder

number_guess.builder: number_guess.c lib.c lib.h
	$(CC) $(CFLAGS) number_guess.c lib.c -o $@

number_guess.capp: number_guess.builder
	./number_guess.builder

zayflight.builder: zayflight.c lib.c lib.h
	$(CC) $(CFLAGS) zayflight.c lib.c -o $@

zayflight.capp: zayflight.builder
	./zayflight.builder

clean:
	rm -f commandline.builder commandline.capp number_guess.builder number_guess.capp zayflight.builder zayflight.capp *.o

.PHONY: all clean