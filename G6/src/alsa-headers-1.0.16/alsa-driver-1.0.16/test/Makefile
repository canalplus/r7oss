CC     	= gcc
CFLAGS	= -O2 -Wall -pipe
TARGETS = mmap_test osspcm osspcm1 ossdelay

all: $(TARGETS)

osspcm: osspcm.c
	$(CC) $(CFLAGS) -o osspcm osspcm.c

osspcm1: osspcm1.c
	$(CC) $(CFLAGS) -o osspcm1 osspcm1.c

osspcm2: osspcm2.c
	$(CC) $(CFLAGS) -o osspcm2 osspcm2.c

ossdelay: ossdelay.c
	$(CC) $(CFLAGS) -o ossdelay ossdelay.c

ossoptr: ossoptr.c
	$(CC) $(CFLAGS) -o ossoptr ossoptr.c

mmap_test: mmap_test.c
	$(CC) $(CFLAGS) -DVERBOSE -o mmap_test mmap_test.c -lm

seq1: seq1.c
	$(CC) $(CFLAGS) -DVERBOSE -o seq1 seq1.c

seq2: seq2.c
	$(CC) $(CFLAGS) -DVERBOSE -o seq2 seq2.c

clean:
	rm -f *.o $(TARGETS)

mrproper: clean
	rm -f *~ *.orig *.rej .#*
