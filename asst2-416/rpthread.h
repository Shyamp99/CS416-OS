// File:	rpthread_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef RTHREAD_T_H
#define RTHREAD_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_RTHREAD macro */
#define USE_RTHREAD 1

#define STACK_SIZE SIGSTKSZ

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

typedef uint rpthread_t;

int t_count = 1;

ucontext_t sched_context;

typedef struct threadControlBlock {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...

	int tid;
	int yielded;
	int queue;
	ucontext_t ctx;
	int status; //0 = ready, -1 = exit, 1 = running
	//suseconds_t time_executed; //lower the val higher the prio cuz it's amount of time ran
	unsigned int time_executed;
	struct threadControlBlock* next;
	uint user_id;
	struct timeval exec_start;
	// YOUR CODE HERE
} TCB;

/* mutex struct definition */
typedef struct rpthread_mutex_t
{
	struct TCB* locker;
	struct rpthread_mutex_t* next;

	// YOUR CODE HERE
} rpthread_mutex_t;

/*typedef struct scheduled_threads_LL{
	int tid;
	TCB* thread_tcb;
	scheduled_threads* next = NULL;
	double prio_level = 0
	suseconds_t time_executed = 0; //lower the val higher the prio cuz it's amount of time ran
} scheduled_thread;*/

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


/* Function Declarations: */

/* create a new thread */
int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int rpthread_yield();

/* terminate a thread */
void rpthread_exit(void *value_ptr);

/* wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr);

/* initial the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex);

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex);

/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex);

static void sched_stcf();

static void sched_mlfq();

#ifdef USE_RTHREAD
#define pthread_t rpthread_t
#define pthread_mutex_t rpthread_mutex_t
#define pthread_create rpthread_create
#define pthread_exit rpthread_exit
#define pthread_join rpthread_join
#define pthread_mutex_init rpthread_mutex_init
#define pthread_mutex_lock rpthread_mutex_lock
#define pthread_mutex_unlock rpthread_mutex_unlock
#define pthread_mutex_destroy rpthread_mutex_destroy
#endif

#endif