COREUTIL=../coreutils-8.23
IDIR=$(COREUTIL)/lib -I$(COREUTIL)/src
LDIR=/usr/local/lib
OBJS=jmp_logpipe.c cmdParser.o
DEPS=jmp_logpipe.c cmdParser.o
CC=gcc
CFLAGS=-c -std=gnu99 -Wall
LDFLAGS=-O0 -g -Wall -Wl,--as-needed -D_FILE_OFFSET_BITS=64 -L/usr/local/lib $(COREUTIL)/src/libver.a $(COREUTIL)/lib/libcoreutils.a $(COREUTIL)/lib/libcoreutils.a $(COREUTIL)/lib/fclose.o $(COREUTIL)/lib/fflush.o $(COREUTIL)/lib/fseeko.o $(COREUTIL)/lib/fadvise.o $(COREUTIL)/lib/progname.o
TARGET=jmp_logpipe

all: $(TARGET)
	@echo done.

%.o: %.c $(DEPS)
	$(CC) -L$(LDIR) -I$(IDIR) -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -L$(LDIR) -I$(IDIR) -Isrc/ $(OBJS) -o $@

cmdParser.o: cmdParser.rl
	ragel -G2 -C -o cmdParser.c cmdParser.rl
	$(CC) -o $@ cmdParser.c $(CFLAGS)

clean:
	rm *.o cmdParser.cpp -f
	rm $(TARGET) -f
