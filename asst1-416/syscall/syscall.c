/* syscall.c
 *
 * Group Members Names and NetIDs:
 *   1. Shyam Patel spp128
 *   2. Amali Delauney ajd298
 *
 * ILab Machine Tested on: java
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/syscall.h>

double avg_time = 0;

int main(int argc, char *argv[]) {

    /* Implement Code Here */

    // Remember to place your final calculated average time
    // per system call into the avg_time variable
	int i;
	struct timeval tv;
	time_t curr;
	time_t new_t;
	for(i = 0; i < 5000000; i++){
		gettimeofday(&tv, NULL);
		curr = tv.tv_usec+tv.tv_sec;	
		syscall(SYS_getuid);
		gettimeofday(&tv, NULL);
		new_t = tv.tv_usec+tv.tv_sec;
		avg_time += curr - new_t;
	}
    //printf("%f\n", avg_time);
	avg_time /= 5000000;
	printf("Average time per system call is %f microseconds\n", avg_time);

    return 0;
}
