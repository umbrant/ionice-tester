#include "ionicer.h"

static FILE * log;

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
      fprintf(log, "Seek %ld us\n", t2_usec - t1_usec);
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

  // Start two child threads that do random read and sequential scan
  pthread_t threads[5];
  pthread_attr_t attrs;
  pthread_attr_init(&attrs);

  param_t boosted_param;
  boosted_param.duration = duration;
  boosted_param.filename = f1;
  boosted_param.type = SEEK;
  boosted_param.priority = 1;

  param_t worker_param;
  worker_param.duration = duration;
  worker_param.filename = f2;
  worker_param.type = SEEK;
  worker_param.priority = -1;

  ret = pthread_create(&threads[0], &attrs, reader, (void*)&boosted_param);
  int i;
  for(i=1; i<5; i++) {
    ret = pthread_create(&threads[i], &attrs, reader, (void*)&worker_param);
  }

  for(i=0; i<5; i++) {
    pthread_join(threads[i], NULL);
  }

  /*

  pthread_t rand_read, rand_scan;
  pthread_attr_t attrs;
  pthread_attr_init(&attrs);

  param_t scan_param;
  scan_param.duration = duration; 
  scan_param.filename = f1;
  scan_param.type = SCAN;
  scan_param.priority = 0;

  param_t seek_param;
  seek_param.duration = duration; 
  seek_param.filename = f2;
  seek_param.type = SEEK;
  if(seek_priority) {
    seek_param.priority = 1;
  } else {
    seek_param.priority = 0;
  }

  ret = pthread_create(&rand_read, &attrs, reader, (void*)&seek_param);
  ret = pthread_create(&rand_scan, &attrs, reader, (void*)&scan_param);

  // Wait for completion
  pthread_join(rand_read, NULL);
  pthread_join(rand_scan, NULL);
  */

  return 0;
}
