#include <stdio.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <string.h>
#include <stdlib.h>

#define BUFSIZE (64*1024) /* 64kb */

/* must be equal to s3 multipart_chunk_size_mb, see $HOME/.s3cfg */
#define CHUNKSIZE_DEFAULT (15*1024*1024)

void bintohex(char * dst, unsigned char * src) {
  int i;
  for (i=0; i<16; i++) sprintf(dst+(i*2),"%02x",src[i]);
  dst[32] = 0;
}

int main (int argc, char ** argv) {
  MD5_CTX c, d, e;
  unsigned char md[16], *buf;
  char md5s[33], md5t[70];
  ssize_t i, size = 0, chunks = 0, chunksize = 0;
  ssize_t chunksize_cfg = CHUNKSIZE_DEFAULT;

  if (getenv("CHUNKSIZE") != NULL) {
    chunksize_cfg = atoi(getenv("CHUNKSIZE"));
  }

  if ((chunksize_cfg < 0) ||
      ((chunksize_cfg % BUFSIZE) != 0)) {
    fprintf(stderr,"md5tee: ERROR: CHUNKSIZE must be multiple of %d or 0 to disable.\n",BUFSIZE);
    return 1;
  }
  
  if (argc >= 2) {
    if ((strcmp(argv[1],"-h") == 0) ||
	(strcmp(argv[1],"--help") == 0)) {
      printf("Description: pipes content from stdin to stdout and calculates its size and md5sum\n\n"
	     "Usage: command1 | md5tee | command2\n"
	     "Description: pipes from command1 to command2. writes size and md5sum to stderr\n\n"
	     "Usage: command1 | md5tee filename | command2\n"
	     "Description: pipes from command1 to command2. writes size and md5sum to filename\n"
	     "Note: md5tee will create the output file, it must not exists previously\n\n"
	     "Output format: SIZE MD5SUM S3MD5SUM\n"
	     "s3md5sum is md5 in s3-storage format: md5-#chunks(ChunkSizeBytes)\n"
	     "use enviroment var CHUNKSIZE to change it. CHUNKSIZE=0 will disable s3md5\n"
	     "default CHUNKSIZE is %i bytes\n\n"
	     "This program is useful when the file is too big and there is not enough disk space in local filesystem\nExample: tar -cf (big content) | md5tee save_the_md5_locally.txt | s3cmd put s3://somewhere\n\n"
	     "(c) 2020 Pedro Zorzenon Neto https://pzn.com.br/md5tee\n", CHUNKSIZE_DEFAULT);
      return 0;
    }
    if ((strcmp(argv[1],"-v") == 0) ||
	(strcmp(argv[1],"--version") == 0)) {
      printf("md5tee version 1.0\n");
      return 0;
    }
    if (access(argv[1],F_OK) == 0) {
      fprintf(stderr,"md5tee: ERROR: output file '%s' exists. "
	      "please remove it before running.\n",argv[1]);
      return 1;
    }
  }
  
  MD5_Init(&c);
  MD5_Init(&d);
  MD5_Init(&e);

  buf = malloc(BUFSIZE);

  do {
    i = read(STDIN_FILENO, buf, BUFSIZE);
    if (i > 0) {
      size += i;
      MD5_Update(&c, buf, i);
      if (chunksize_cfg != 0) {
	if (chunksize + i >= chunksize_cfg) {
	  MD5_Update(&d, buf, chunksize_cfg - chunksize);
	  MD5_Final(md, &d);
	  MD5_Update(&e, md, 16);
	  MD5_Init(&d);
	  if ((chunksize + i - chunksize_cfg) > 0) {
	    MD5_Update(&d, buf + chunksize_cfg - chunksize, chunksize + i - chunksize_cfg);
	    chunksize = chunksize + i - chunksize_cfg;
	  } else {
	    chunksize = 0;
	  }
	  chunks++;
	} else {
	  MD5_Update(&d, buf, i);
	  chunksize += i;
	}
      }
      if (write(STDOUT_FILENO, buf, i) != i) {
	perror("md5tee: ERROR writing to stdout");
	return 2;
      }
    }
  } while (i > 0);

  if ((chunksize_cfg != 0) && (chunksize >= 0)) {
    chunks++;
    MD5_Final(md, &d);
    MD5_Update(&e, md, 16);
  }

  MD5_Final(md, &c);
  bintohex(md5s,md);

  if (chunksize_cfg == 0) {
    md5t[0] = 0;
  } else {
    MD5_Final(md, &e);
    md5t[0] = 0x20;
    bintohex(md5t+1,md);
    sprintf(md5t+33,"-%lu(%lu)",chunks,chunksize_cfg);
  }

  if (argc < 2) {
    fprintf(stderr,"%lu %s%s\n", size, md5s, md5t);
  } else {
    FILE * fh;
    fh = fopen(argv[1],"w");
    if (fh == NULL) {
      perror("md5tee: ERROR creating output file");
      return 3;
    }
    fprintf(fh,"%lu %s%s\n", size, md5s, md5t);
    fclose(fh);
  }
  return 0;
}
