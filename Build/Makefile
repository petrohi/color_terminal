CC = gcc
CFLAGS = -Wall -g -std=c99 -I../Core/Inc
LDFLAGS = -lm

../Core/Inc/luminance_table.h: gen_luminance
	./gen_luminance > $@

gen_luminance: gen_luminance.o rgb.o
	$(CC) $(LDFLAGS) -o $@ $^

gen_luminance.o: gen_luminance.c
	$(CC) $(CFLAGS) -o $@ -c $<

rgb.o: ../Core/Src/rgb.c
	$(CC) $(CFLAGS) -o $@ -c $<
