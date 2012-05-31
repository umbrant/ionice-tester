#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>

#define IOPRIO_PRIO_VALUE(class, prio) (prio | class << 13)
#define IOPRIO_PRIO_CLASS(mask) (mask >> 13)
#define IOPRIO_PRIO_DATA(mask) (mask & 0xF)

enum {
  SEEK,
  SCAN,
};

typedef struct {
  char* filename;
  int duration;
  int type;
  int priority;
} param_t;

enum {
	IOPRIO_CLASS_NONE,
	IOPRIO_CLASS_RT,
	IOPRIO_CLASS_BE,
	IOPRIO_CLASS_IDLE,
};

enum {
	IOPRIO_WHO_PROCESS = 1,
	IOPRIO_WHO_PGRP,
	IOPRIO_WHO_USER,
};

int main(int argc, char *argv[]);
void raise_priority();
void lower_priority();


