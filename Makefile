all: ionicer
	
ionicer:ionicer.c ionicer.h
	gcc -g -ggdb -o ionicer -pthread -lrt ionicer.c
clean:
	rm -f ionicer
