#include "ionicer.h"

void* reader(void *arg) {
  param_t* params = (param_t*) arg;
  int ret;
  int tid = gettid();

  int fd = open(params->filename, O_RDONLY);
  if(fd == -1) {
    perror("Reader open");
    return;
  }
  else {
    printf("[%d] Reader opened file %s\n", tid, params->filename);
  }

  int bufsize;
  switch(params->type) {
    case SEEK:
      printf("[%d] Doing random reads\n", tid);
      bufsize = 4*1024;
      break;
    case SCAN:
      printf("[%d] Doing random scans\n", tid);
      bufsize = 4*1024*1024;
      break;
  }

  if(params->priority > 0) {
    raise_priority();
  }
  else if(params->priority < 0) {
    lower_priority();
  }

  time_t now = time(NULL);
  time_t start = now;
  time_t end = now + params->duration;

  // IO size

  char buffer[bufsize];
  int count = 0;

  struct stat st;
  fstat(fd, &st);
  off_t size = st.st_size;
  int blocks = size / bufsize;

  while(now < end) {
    // Time the I/O

    struct timeval t1, t2;
    gettimeofday(&t1, NULL);

    // seek around randomly
    long int seek_off = (random()%blocks)*bufsize;
    lseek(fd, seek_off, SEEK_SET);
    int total = 0;
    while(total < bufsize) {
      int num_bytes = read(fd, buffer+total, bufsize-total);
      if(num_bytes < 0) {
        perror("read");
        return;
      }
      total += num_bytes;
    }
    gettimeofday(&t2, NULL);
    long t1_usec = (100000 * t1.tv_sec) + t1.tv_usec;
    long t2_usec = (100000 * t2.tv_sec) + t2.tv_usec;
    if(params->type == SEEK) {
      fprintf(stderr, "Seek %ld us\n", t2_usec - t1_usec);
    }

    now = time(NULL);
    count++;
  }

  time_t finished = time(NULL);
  time_t duration = finished - start;
  printf("[%d] Read ops/s: %.2f\n", gettid(), (float)count / duration);
  close(fd);
}

int gettid() {
  return syscall(SYS_gettid);
}

void lower_priority() {
  int tid = gettid();
  int new_prio = 7;
  int new_class = IOPRIO_CLASS_IDLE;

  int ret = syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, tid, IOPRIO_PRIO_VALUE(new_class, new_prio));
  if(ret < 0) {
    printf("Error trying to set priority of tid %d\n", tid);
    perror("ioprio_set");
  }
  else {
    printf("Lowered priority of thread %d\n", tid);
  }
}

void raise_priority() {
  int tid = gettid();
  int new_prio = 0;
  int new_class = IOPRIO_CLASS_RT;

  int ret = syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, tid, IOPRIO_PRIO_VALUE(new_class, new_prio));
  if(ret < 0) {
    printf("Error trying to set priority of tid %d\n", tid);
    perror("ioprio_set");
  }
  else {
    printf("Raised priority of thread %d\n", tid);
  }
}

int main(int argc, char *argv[]) {

  int ret;

  /*
  int c;

	while ((c = getopt(argc, argv, "+p:c:t:")) != EOF) {
		switch (c) {
		case 'p':
			new_prio = strtol(optarg, NULL, 10);
			break;
		case 'c':
			new_class = strtol(optarg, NULL, 10);
			break;
		case 't':
			tid = strtol(optarg, NULL, 10);
			break;
		}
	}

	switch (new_class) {
		case IOPRIO_CLASS_NONE:
			new_class = IOPRIO_CLASS_BE;
			break;
		case IOPRIO_CLASS_RT:
		case IOPRIO_CLASS_BE:
			break;
		case IOPRIO_CLASS_IDLE:
			new_prio = 7;
			break;
		default:
			printf("bad prio class %d\n", new_class);
			return 1;
	}
	*/

  // Start two child threads that do random read and sequential scan
  pthread_t rand_read, rand_scan;
  pthread_attr_t attrs;
  pthread_attr_init(&attrs);

  param_t scan_param;
  scan_param.duration = 30; 
  scan_param.filename = "/home/andrew/4G.temp";
  scan_param.type = SCAN;
  scan_param.priority = 0;

  param_t seek_param;
  seek_param.duration = 30; 
  seek_param.filename = "/home/andrew/1G.temp";
  seek_param.type = SEEK;
  if(argc > 1) {
    seek_param.priority = 1;
  } else {
    seek_param.priority = 0;
  }

  ret = pthread_create(&rand_read, &attrs, reader, (void*)&seek_param);
  ret = pthread_create(&rand_scan, &attrs, reader, (void*)&scan_param);

  // Wait for completion
  pthread_join(rand_read, NULL);
  pthread_join(rand_scan, NULL);

  return 0;
}