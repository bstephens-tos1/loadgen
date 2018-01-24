# loadgen
## A random read and sequential write generator designed to run workloads for ftrace.

loadgen performs a variable number of writes to/reads from a given
path. Because it was designed to be create workloads to be traceable by ftrace,
loadgen contains code that toggles ftrace on and off at the relevant moments of
program execution.

The "usage" output of the program:

```
Usage: ./loadgen <file> <num_bytes> <num_io> <delay(us)> [-r]
file: the path of the target file
num_bytes: the size of each IO request
num_io: the number of IO requests
delay: usleep between I/O requests
-r: perform reads instead of writes
SEQUENCE_SIZE=8
```

Some example runs of loadgen: </br>
```
./loadgen /mnt/ssd/bigfile 4096 100 0 -r
./loadgen /mnt/rd/bigfile 1024000 100 0
```

When writing, a buffer will have a sequence number embedded at the front
followed by 0xDEADBEEF, which is written until the buffer is full.  You can, of
course, change what is written to the buffer to suit your own needs.

loadgen prefers random reads over sequential reads to avoid the caching effects
associated with sequential reads.
