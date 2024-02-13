# viktar Makefile, Henry Sides, Lab2, CS333
CC = gcc
LFLAGS = -lz
CFLAGS = -fno-stack-protector -Wall -Wshadow -Wunreachable-code -Wredundant-decls -Wmissing-declarations -Wold-style-definition -Wmissing-prototypes -Wdeclaration-after-statement -Wextra -Werror -Wpedantic -Wuninitialized -Wunsafe-loop-optimizations -Wno-return-local-addr -g 
PROG = viktar

all: $(PROG)

$(PROG): $(PROG).o
	$(CC) $(CFLAGS) -o $@ $< $(LFLAGS)

$(PROG).o: $(PROG).c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(PROG) \#*
