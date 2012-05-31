all: ionicer
	
ionicer:ionicer.c ionicer.h
	gcc -o ionicer -pthread -lrt ionicer.c
clean:
	rm -f ionicer
