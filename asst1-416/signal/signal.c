/* signal.c
 *
 * Group Members Names and NetIDs:
 *   1. Shyam Patel spp128
 *   2. Amali Delauney ajd298
 *
 * ILab Machine Tested on: java
 *
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/* Part 1 - Step 2 to 4: Do your tricks here
 * Your goal must be to change the stack frame of caller (main function)
 * such that you get to the line after "r2 = *( (int *) 0 )"
 */
void segment_fault_handler(int signum) {

    printf("I am slain!\n");


    /* Implement Code Here */
	
	(&signum)[47]+=2;

}

int main(int argc, char *argv[]) {

    int r2 = 0;

    /* Part 1 - Step 1: Registering signal handler */
    /* Implement Code Here */
	signal(SIGSEGV,segment_fault_handler);

    r2 = *( (int *) 0 ); // This will generate segmentation fault

    printf("I live again!\n");

    return 0;
}
