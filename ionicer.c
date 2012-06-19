#include "ionicer.h"

static FILE * log;

void* reader(void *arg) {
  param_t* params = (param_t*) arg;
  int ret;
  int tid = gettid();

  int fd = open(params->filename, O_RDONLY|O_DIRECT);
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
      bufsize = 64*1024;
      break;
    case SCAN:
      printf("[%d] Doing random scans\n", tid);
      bufsize = 4*1024*1024;
      break;
  }

  set_priority(params->priority);

  long start, now, end;
  struct timeval t;
  gettimeofday(&t, NULL);
  start = (1000000*t.tv_sec) + t.tv_usec;
  end = start + (1000000 * params->duration);
  now = start;

  // IO size

  char * buffer;
  ret = posix_memalign((void**)&buffer, 4096, bufsize);
  if(ret < 0) {
    printf("ERROR with posix_memalign: %d\n", ret);
    return;
  }
  int count = 0;

  struct stat st;
  fstat(fd, &st);
  off_t size = st.st_size;
  int blocks = size / bufsize;

  long max_latency = 0;

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
    long t1_usec = (1000000 * t1.tv_sec) + t1.tv_usec;
    long t2_usec = (1000000 * t2.tv_sec) + t2.tv_usec;
    long latency = t2_usec - t1_usec;
    if(latency > max_latency) {
        max_latency = latency;
    }

    /*
    if(params->type == SEEK) {
      fprintf(log, "Seek %ld us\n", latency);
    }
    */

    now = t2_usec;
    count++;
  }

  long finished = now;
  double duration = (double)(finished - start) / 1000.0 / 1000.0;
  double tput = ((double)count * bufsize) / 1024.0 / duration;
  fprintf(log, "[%d] Priority: %d, Max latency: %.2f ms, Tput: %.2f KiB/s\n",
          gettid(), params->priority, (double)max_latency/1000.0, tput);

  free(buffer);
  close(fd);
}

int gettid() {
  return syscall(SYS_gettid);
}

void set_priority(int new_prio) {
  int tid = gettid();
  int new_class = IOPRIO_CLASS_BE;

  int ret = syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, tid, IOPRIO_PRIO_VALUE(new_class, new_prio));
  if(ret < 0) {
    printf("Error trying to set priority of tid %d\n", tid);
    perror("ioprio_set");
  }
  else {
    printf("Set priority of thread %d to %d\n", tid, new_prio);
  }
}

int main(int argc, char *argv[]) {

  int ret;

  int c;

  int duration = 60;
  int seek_priority = 0;
  char* logfile = NULL;

	while ((c = getopt(argc, argv, "d:o:p")) != EOF) {
		switch (c) {
		case 'd':
		  duration = strtol(optarg, NULL, 10);
		  break;
		case 'o':
		  logfile = optarg;
		  break;
		case 'p':
			seek_priority = 1;
			break;
		}
	}
	char *f1, *f2;
	if(argc - optind == 2) {
	  f1 = argv[optind];
	  f2 = argv[optind+1];
	}
	else {
	  printf("Usage: ionicer [-d duration] [-o outfile] [-p] scan_file seek_file\n");
	  return 1;
	}

	if(logfile == NULL) {
	  log = stderr;
    printf("Defaulting to stderr for logging\n");
	} else {
	  log = fopen(logfile, "w");
	  if(log == NULL) {
	    perror("Cannot open log file");
	    log = stderr;
	  }
	  else {
      printf("Using %s for logging\n", logfile);
	  }
	}

  int num_threads = 8;

  // Start two child threads that do random read and sequential scan
  pthread_t threads[num_threads];
  pthread_attr_t attrs[num_threads];
  param_t params[num_threads];

  int i;
  for(i=0; i<num_threads; i++) {
    params[i].duration = duration;
    params[i].filename = f1;
    params[i].type = SEEK;
    params[i].priority = i;

    pthread_attr_init(&attrs[i]);
    ret = pthread_create(&threads[i], &attrs[i], reader, (void*)&params[i]);
  }

  for(i=0; i<num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
