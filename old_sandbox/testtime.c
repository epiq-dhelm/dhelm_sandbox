
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>

#define BILLION 1000000000

/* add a time in nanosecends to timespec */
int timeval_add(struct timespec *result, struct timespec *in, long int addr)
{
    long int secs, nsecs, new_nsecs, new_secs;

    // calculate addr seconds
    secs = addr / BILLION;

    nsecs = addr - (secs * BILLION);

    new_nsecs = in->tv_nsec + nsecs;

    printf("addr secs %ld, nsecs %ld, new_nsecs %ld\n", secs, nsecs, new_nsecs);

    if (new_nsecs > BILLION)
    {
        new_secs = new_nsecs / BILLION;
        new_nsecs = new_nsecs - (new_secs * BILLION);
    }

    new_secs = in->tv_sec + secs;

    result->tv_sec = new_secs;
    result->tv_nsec = new_nsecs;

    return 0;
}



/* Subtract the ‘struct timeval’ values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */

int
timeval_subtract (struct timespec *result, struct timespec *x, struct timespec *y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_nsec < y->tv_nsec) {
    int nsec = (y->tv_nsec - x->tv_nsec) / 1000000 + 1;
    y->tv_nsec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_nsec - y->tv_nsec > 1000000) {
    int nsec = (x->tv_nsec - y->tv_nsec) / 1000000;
    y->tv_nsec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_nsec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

int main (void)
{
    struct timespec start;
    struct timespec stop;
    struct timespec result;
    struct timespec restime;
    int ret;

    ret = clock_getres(CLOCK_REALTIME, &restime);

    printf("resolution time ret %d, sec %ld, ns %ld\n", ret, restime.tv_sec, restime.tv_nsec); 

    ret = clock_gettime(CLOCK_REALTIME, &start);
    printf("start time ret %d, sec %ld, ns %ld\n", ret, start.tv_sec, start.tv_nsec); 

    sleep(1);

    ret = clock_gettime(CLOCK_REALTIME, &stop);
    printf("stop time ret %d, sec %ld, ns %ld\n", ret, stop.tv_sec, stop.tv_nsec); 

    ret = timeval_subtract(&result, &stop, &start);
    printf("diff time ret %d, sec %ld, ns %ld\n", ret, result.tv_sec, result.tv_nsec); 

    ret = timeval_add(&result, &stop, 2 * BILLION);
    printf("time add ret %d, sec %ld, ns %ld\n", ret, result.tv_sec, result.tv_nsec); 



}

