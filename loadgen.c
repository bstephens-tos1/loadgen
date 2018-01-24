#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#define SEQUENCE_SIZE  (sizeof(int)*2)

static char* TOGGLE_PATH = "/sys/kernel/debug/tracing/tracing_on";

/*
 * Write a 1 to fd.  fd is assumed to refer to TOGGLE_PATH, which should
 * only have a 0 or 1 written to it.
 *
 * Return 1 on success, 0 on failure
 */
int startTrace(int fd)
{
	char* on = "1";
	if(write(fd, on, strlen(on)) != strlen(on))
		return 0;

	return 1;
}

/*
 * Write a 0 to fd.  fd is assumed to refer to TOGGLE_PATH, which should
 * only have a 0 or 1 written to it.
 *
 * Return 1 on success, 0 on failure
 */
int stopTrace(int fd)
{
	char* off = "1";
	if(write(fd, off, strlen(off)) != strlen(off))
		return 0;

	return 1;
}

int writeSequences(char* fileName, int bytes, int writes, int delay, int trace_toggle_fd)
{
	// The sequence_num number, which should be embedded at the beginning of a buffer
	// that will be written to disk.  This value should be unique for each new
	// write()
	unsigned int sequence_num = 0x11111111;

	int outfile_fd;

	// Need O_DIRECT to avoid cache effects?
	// TODO:  Adding in O_DIRECT causes writes not to be seen at the block layer.  Why?
	// This seems to be interfering with the intended effect of using fsync()
	if((outfile_fd = open(fileName, O_CREAT|O_WRONLY/*|O_DIRECT|O_SYNC*/, 0600)) < 0) {
		perror(fileName);
		return 1;
	}

	char *buff = malloc(bytes);

	if (!buff) {
		perror("malloc()\n");
		return 0;
	}

	*(unsigned int*)buff = sequence_num;

	int deadbeef = 0xDEADBEEF;

	// Writes DEADBEEF into the buffer over and over after the sequence num,
	// so that our buffer is not completely filled with 0's.
	for(int i = sizeof(int); i < bytes - sizeof(int); i += sizeof(int)) {
		*(unsigned int*)(buff + i) = deadbeef;
	}

	#ifdef DEBUG
		printf("bytes: %u sizeof(buff): %lu\n", bytes, sizeof(buff));
		printf("Buffer: ");
		for(int i = 0; i < bytes; ++i)
			printf("%X", buff[i]);
		printf("\n");
	#endif

	int ret = 0;

	while(writes--) {
		if(!startTrace(trace_toggle_fd)) {
			printf("Failure toggling before write to %s \n", TOGGLE_PATH);
			ret = 1;
			break;
		}

		write(outfile_fd, buff, bytes);

		// Flush it all to disk.  Without this fsync(), we cannot ensure that
		// all writes will go to the block layer before tracing ends.
		fsync(outfile_fd);

		if(!stopTrace(trace_toggle_fd)) {
			printf("Failure toggling after write to %s \n", TOGGLE_PATH);
			ret = 1;
			break;
		}

		// If there should be a delay, it must be done before the next write()
		if (delay > 0) {
			usleep(delay);
		}

		// Incrememnt the sequence_num number in case there is another write()
		sequence_num++;
	}

	close(outfile_fd);
	close(trace_toggle_fd);
	free(buff);

	return ret;
}

int readSequences(char* fileName, int bytes, int reads, int delay, int trace_toggle_fd)
{
	int infile_fd;

	if((infile_fd = open(fileName, O_RDONLY) < 0)) {
		perror(fileName);
		return 1;
	}

	char *buff = malloc(bytes);

	if (!buff) {
		perror("malloc()\n");
		return 0;
	}

	int ret = 0;

	while(reads--) {
		if(!startTrace(trace_toggle_fd)) {
			printf("Failure toggling before write to %s \n", TOGGLE_PATH);
			ret = 1;
			break;
		}

		read(infile_fd, buff, bytes);

		if(!stopTrace(trace_toggle_fd)) {
			printf("Failure toggling after write to %s \n", TOGGLE_PATH);
			ret = 1;
			break;
		}

		// If there should be a delay, it must be done before the next read()
		if (delay > 0) {
			usleep(delay);
		}
	}

	close(infile_fd);
	close(trace_toggle_fd);
	free(buff);

	return ret;
}

int main(int argc, char** argv)
{
	// Do not assume that ftrace is disabled when this program is launched.
	// Before we do anything at all, we need to try and disable ftrace to
	// minimize the amount of unecessary operations that are traced
	int trace_toggle_fd;

	if((trace_toggle_fd = open(TOGGLE_PATH, O_WRONLY)) < 0) {
		perror(TOGGLE_PATH);
		close(trace_toggle_fd);
		return 1;
	}

	if(!stopTrace(trace_toggle_fd)) {
		printf("Failure toggling at startup to %s \n", TOGGLE_PATH);
		close(trace_toggle_fd);
		return 1;
	}

	if(argc < 5) {
		printf("Usage: %s <file> <num_bytes> <num_io> <delay(us)> [-DDEBUG=1]\n"
			"file: the path of the target file\n"
			"num_bytes: the size of each IO request\n"
			"num_io: the number of IO requests\n"
			"delay: the sleeping time between IO requests. 0 means no sleep.\n"
			"-r: perform reads instead of writes"
			"SEQUENCE_SIZE=%lu\n", argv[0], SEQUENCE_SIZE
			);
		return 1;
	}

	char *fileName = argv[1];
	int bytes = atoi(argv[2]);
	int num_io = atoi(argv[3]);
	int delay = atoi(argv[4]);

	int option = 0;
	int READ = 0;
	if((option = getopt(argc, argv, "r")) != -1) {
		READ = 1;
	}

	if (bytes <= SEQUENCE_SIZE || bytes % sizeof(int) != 0 || num_io <= 0 || delay < 0) {
		printf("Bytes must exceed and be a multiple of %lu, num_io must be 1 or greater,"
				"and delay cannot be negative.\n", SEQUENCE_SIZE);
		return 1;
	}

	if(READ) {
		return readSequences(fileName, bytes, num_io, delay, trace_toggle_fd);
	}
	return writeSequences(fileName, bytes, num_io, delay, trace_toggle_fd);
}
