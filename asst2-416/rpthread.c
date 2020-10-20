// File:	rpthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "rpthread.h"

int initialized = 0;
int sched = 0;
struct itimerval timer;
#define INTERVAL 1 //number of milliseconds
int reload = 0;
int locked = 0;

// Q1.priority > Q4.priority
rpthread_mutex_t* mutexList = NULL;

TCB* queue_L1 = NULL;
TCB* queue_L2 = NULL;
TCB* queue_L3 = NULL;
TCB* queue_L4 = NULL;
TCB* current = NULL;

ucontext_t schedctx;
void* schedstack;

uint get_execution_time (struct timeval start, struct timeval end){
	return (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec; 
}

void update_time(TCB* new_thread){
	struct timeval end;
	gettimeofday(&end, NULL);
	current -> time_executed += get_execution_time(current->exec_start, end);
	if(new_thread != NULL){
		gettimeofday(&end, NULL);
		new_thread -> exec_start = end;
	}
}

void add_to_LL1(TCB* node){
	if (queue_L1 == NULL || queue_L1 == queue_L2){
		queue_L1 = node; 
		node->next = queue_L2;
	}
	else{
		TCB* temp = queue_L1;
	while(temp-> next != NULL && temp-> next != queue_L2){
			if (temp-> time_executed < node -> time_executed && temp-> next-> time_executed > node-> time_executed ){
				node->next = temp->next;
				temp->next = node;
				return;
			}
			temp = temp->next;
		}
		temp->next = node;
	}
}

void add_to_LL2(TCB* node){
	if (queue_L2 == NULL || queue_L2 == queue_L3){
		queue_L2 = node; 
		node->next = queue_L3;
	}
	
	else{
		TCB* temp = queue_L2;
	while(temp-> next != NULL && temp-> next != queue_L3){
			if (temp-> time_executed < node -> time_executed && temp-> next-> time_executed > node-> time_executed ){
				node->next = temp->next;
				temp->next = node;
				return;
			}
			temp = temp->next;
		}
		temp->next = node;
	}
}

void add_to_LL3(TCB* node){
	if (queue_L3 == NULL || queue_L3 == queue_L4){
		queue_L3 = node;
		node->next = queue_L4;
	}
	else{
		TCB* temp = queue_L3;
	while(temp-> next != NULL && temp-> next != queue_L4){
			if (temp-> time_executed < node -> time_executed && temp-> next-> time_executed > node-> time_executed ){
				node->next = temp->next;
				temp->next = node;
				return;
			}
			temp = temp->next;
		}
		temp->next = node;
	}
}

void add_to_LL4(TCB* node){
	if (queue_L4 == NULL){
		queue_L4 = node;
	}
	else{
		TCB* temp = queue_L4;
	while(temp->next != NULL){
			if (temp-> time_executed < node -> time_executed && temp-> next-> time_executed > node-> time_executed ){
				node->next = temp->next;
				temp->next = node;
				return;
			}
			temp = temp->next;
		}
		temp->next = node;
	}
}


TCB* find_TCB(rpthread_t thread){
	TCB* temp = queue_L1;
	while(temp != NULL){
		printf("%d\n", temp->user_id);
		if (temp -> user_id == thread){
			return temp;
		}
		temp = temp -> next;
	}
	return NULL;

}

/*
TCB* find_thread(int TID, uint UID){
	TCB* temp = queue_L1;
	while(temp != NULL){
		if(temp -> tid == TID || temp -> user_id == UID ){
			return temp;
		}
		temp = temp -> next;
	}
	return NULL;
}*/

/* create a new thread */
int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {
    // Create Thread Control Block
    // Create and initialize the context of this thread
    // Allocate space of stack for this thread to run
    // after everything is all set, push this thread int
    
	if (function == NULL){
		return 1;
	}
	TCB* curr_tcb;

	curr_tcb = (TCB*)malloc(sizeof(TCB));
	void* stack = malloc(STACK_SIZE);
	curr_tcb-> tid = t_count;
	t_count++;
	curr_tcb -> user_id = *thread;
	curr_tcb-> status = 0;
	curr_tcb-> next = NULL;
	curr_tcb-> time_executed = 0;

	ucontext_t temp;

	if (getcontext(&temp) < 0 ){
		perror("getcontext");
		return -1;
	}
	//setting tcb -> cctx vars
	temp.uc_link = NULL;
	temp.uc_stack.ss_sp = stack;
	temp.uc_stack.ss_size=STACK_SIZE;
	temp.uc_stack.ss_flags = 0;
	curr_tcb -> ctx = temp;
		
	if (arg == NULL){
		makecontext(&(curr_tcb->ctx), (void *) function, 0, NULL);
	}
	else{
		makecontext(&(curr_tcb->ctx), (void *) function, 1, arg);
	}
	
	add_to_LL1(curr_tcb);
	
	if(initialized == 0)
	{
		setUp();
	}

    return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int rpthread_yield() {
	
	// Change thread state from Running to Ready
	// Save context of this thread to its thread control block
	// switch from thread context to scheduler context
	
	//1. set curr tcb status to be 0 
	//2. take the current context and swap it with top of the queue
	//		swap(first, second) -> context of second gets stored in the first
	getcontext(&schedctx);
    schedctx.uc_stack.ss_sp = schedstack;
    schedctx.uc_stack.ss_size = STACK_SIZE;
    schedctx.uc_stack.ss_flags = 0;
	
	current-> status = 0;
	current->yielded = 1;
	update_time(NULL);
	swapcontext(&(current->ctx), &schedctx);
	return 0;
};

void rpthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread
	// we need it to yeet and restart the timer as well upon the actual murder of a thread
	// make sure the scheduler context is inited prior to any form of execution
	getcontext(&schedctx);
    schedctx.uc_stack.ss_sp = schedstack;
    schedctx.uc_stack.ss_size = STACK_SIZE;
    schedctx.uc_stack.ss_flags = 0;
	
	current-> status = -1;
	ucontext_t temp = current->ctx;
	void * stack = temp.uc_stack.ss_sp;
	free(stack);
	update_time(NULL);
	setcontext(&schedctx);		
	if (value_ptr == NULL){
		return;
	}

};


/* Wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr) {
	
	// Wait for a specific thread to terminate
	// De-allocate any dynamic memory created by the joining thread
 	TCB* thread_tcb = find_TCB(thread);
	// TCB* this_thread = curr_running;
	printf("%d\n", thread);
	if (thread_tcb == NULL){
		puts("uou done fucked up");
	}
	if (thread_tcb-> status == -1){
		return (int) (*value_ptr);
	}
	int val = 0;
	puts("2");
	while(thread_tcb -> status != -1){
		/* iggnore this is for testing
		if(val == 0){
			swapcontext(&(curr_running->ctx), &(t_1->ctx));
		} */
	}
	puts("3");
	//setcontext(&(this_thread->ctx));
	return 0;
};

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr){
	//Initialize data structures for this mutex

	// LOGIC:
	// 1. Basically just a struct current thread that is allowed to run:w
	
	mutex = (rpthread_mutex_t*)malloc(sizeof(rpthread_mutex_t));
	
	mutex->next = mutexList;
	
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex)
{
        // use the built-in test-and-set atomic function to test the mutex
        // When the mutex is acquired successfully, enter the critical section
        // If acquiring mutex fails, push current thread into block list and 
        // context switch to the scheduler thread

        // LOGIC:
		// 1. Get the current context for the currently running thread (need to figure this part out)
		// 2. Go thru the linked list and yeet everything to status 0 
		// 3. Force everything to wait (need to figure this part out)
        mutex->locker = current;
		locked = 1;
		
		return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.
	
	// LOGIC:
	// 1. Go thru the current running LL and set status to 1 and then begin execution for each respective context
	mutex->locker = NULL;
	locked = 0;
	
	return 0;
};


/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init
	free(mutex);
	locked = 0;

	return 0;
};

/* scheduler called when timer reaches 0*/
static void schedule()
{
	// Every time when timer interrup happens, your thread library 
	// should be contexted switched from thread context to this 
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (STCF or MLFQ)
	getcontext(&schedctx);
    schedctx.uc_stack.ss_sp = schedstack;
    schedctx.uc_stack.ss_size = STACK_SIZE;
    schedctx.uc_stack.ss_flags = 0;
	
	puts("interrupt!");

	// save running thread, swap context to scheduler
	//if (sched == STCF)
	//{
		//makecontext(&schedctx, sched_stcf);
		makecontext(&schedctx, (void *) sched_stcf, 0 , NULL);
		swapcontext(&(current->ctx),&schedctx);
	//}
	//else if (sched == MLFQ)
	//{
	//	makecontext(&schedctx, sched_mlfq);
	//	makecontext(&schedctx, (void *) sched_mlfq, 0 , NULL);
	//	swapcontext(current->ctx,&schedctx);
	//}

	// YOUR CODE HERE

// schedule policy
//#ifndef MLFQ
	// Choose STCF
//#else 
	// Choose MLFQ
//#endif

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf()
{
	puts("STCF");
	// Your own implementation of STCF
	// (feel free to modify arguments and return types)
	printf("Old thread's time: %d\n", current->time_executed);
	if(queue_L1 != NULL && locked==0)						// if only 1 thread running, no need to switch it out
	{
		current->status = 0;					// set current ready
		add_to_LL1(current);					// add curr to L1 (reorganizing threads by priority)
		current = queue_L1;						// curr_running = head of LL (shortest time)
		
		current->status = 1;					// set current running
		queue_L1 = queue_L1->next;				// remove current from queue
	}
	printf("New thread's time: %d\n", current->time_executed);
	sleep(1);
	setcontext(&(current->ctx));					// setcontext to current
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq()
{
	
	puts("MLFQ");
	sleep(1);
	++reload;
	
	if(locked == 0)
	{
		if(reload == 100)// times scheduler is called before moving everyone up to Q1
		{
			TCB* temp = queue_L1;
			while(temp != NULL)
			{
				temp->queue = 1;
				temp = temp->next;
			}
			
			queue_L2 = NULL;
			queue_L3 = NULL;
			queue_L4 = NULL;
			
			reload = 0;
		}
		else
		{
			// IF IN Q1
			if(current->queue = 1)
			{
				if(current->yielded != 0)
				{
					current->yielded = 0;
					current->status = 0;					
					add_to_LL1(current);					// add curr to L1
					current = queue_L1;						// choose thread from front of queue
				
					current->status = 1;
					
					queue_L1 = queue_L1->next;				// remove current from queue
				}
				else // yield == 0; thread did not yield; i.e. thread used full time slice; move to L2
				{
					current->yielded = 0;
					current->queue = 2;
					current->status = 0;					
					add_to_LL2(current);					// add curr to L2
					current = queue_L1;						// choose thread from front of queue
				
					current->status = 1;					
					
					if(queue_L1 == queue_L2)				// remove current from queue
					{
						queue_L2 = queue_L2->next;
						queue_L1 = queue_L2;
					}
					else
					{
						queue_L1 = queue_L1->next;
					}		
				}
			}
			
			// IF IN Q2
			else if(current->queue = 2)
			{
				if(current->yielded != 2)
				{
					if(current->yielded == 1)
					{
						current->yielded = 0;
						current->status = 0;
						add_to_LL2(current);
						current = queue_L1;
				
						current->status = 1;
					
						if(queue_L1 == queue_L2)
						{
							queue_L2 = queue_L2->next;
							queue_L1 = queue_L2;
						}
						else
						{
							queue_L1 = queue_L1->next;
						}	
					}
					else if(current->yielded == 0)
					{
						current->yielded = 2;
					}
				}
				else // yield == 2; thread did not yield 2x; i.e. thread used 2 full time slices; move to L3
				{
					current->yielded = 0;
					current->queue = 3;
					current->status = 0;					
					add_to_LL3(current);					// add curr to L3
					current = queue_L1;						// choose thread from front of queue
				
					current->status = 1;					
					
					if(queue_L1 == queue_L3)				// remove current from queue
					{
						queue_L3 = queue_L3->next;
						queue_L2 = queue_L3;
						queue_L1 = queue_L3;
					}
					else
					{
						queue_L1 = queue_L1->next;
					}		
				}
			}
			
			// IF IN Q3
			else if(current->queue = 3)
			{
				if(current->yielded != 3)
				{
					if(current->yielded == 1)
					{
						current->yielded = 0;
						current->status = 0;
						add_to_LL3(current);
						current = queue_L1;
				
						current->status = 1;
					
						if(queue_L1 == queue_L3)
						{
							queue_L3 = queue_L3->next;
							queue_L2 = queue_L3;
							queue_L1 = queue_L3;
						}
						else
						{
							queue_L1 = queue_L1->next;
						}		
					}
					else if(current->yielded == 0)
					{
						current->yielded = 2;
					}
					else if(current->yielded == 2)
					{
						current->yielded = 3;
					}
				}
				else // yield == 3; thread did not yield 3x; i.e. thread used 3 full time slices; move to L4
				{
					current->yielded = 0;
					current->queue = 4;
					current->status = 0;					
					add_to_LL4(current);					// add curr to L4
					current = queue_L1;						// choose thread from front of queue
				
					current->status = 1;					
					
					if(queue_L1 == queue_L4)				// remove current from queue
					{
						queue_L4 = queue_L4->next;
						queue_L3 = queue_L4;
						queue_L2 = queue_L4;
						queue_L1 = queue_L4;
					}
					else
					{
						queue_L1 = queue_L1->next;
					}		
				}
			}
			
			// IF IN Q4
			else if(current->queue = 4)
			{
				if(current->yielded == 1 || current->yielded == 4) //this thread's hit rock bottom
					{
						current->yielded = 0;
						current->status = 0;					// set current ready
						add_to_LL4(current);					// add curr to L1 (reorganizing threads by priority)
						current = queue_L1;						// curr_running = head of LL (shortest time)
				
						current->status = 1;					// set current running
					
						if(queue_L1 == queue_L4)				// remove current from queue
						{
							queue_L4 = queue_L4->next;
							queue_L3 = queue_L4;
							queue_L2 = queue_L4;
							queue_L1 = queue_L4;
						}
						else
						{
							queue_L1 = queue_L1->next;
						}
					}
					else if(current->yielded == 0)
					{
						current->yielded = 2;
					}
					else if(current->yielded == 2)
					{
						current->yielded = 3;
					}
					else if(current->yielded == 3)
					{
						current->yielded = 4;
					}
			}
		}
	}
	
	setcontext(&(current->ctx));							// setcontext to current
}

// Feel free to add any other functions you need

// YOUR CODE HERE
void setUp()
{
	schedstack = malloc(STACK_SIZE);
	if (schedstack == NULL)
	{
        perror("scheduler stack error");
        exit(1);
    }
	
	struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &schedule;
	// sa.sa_sigaction = &schedule; //try this instead?
    sigaction(SIGPROF, &sa, NULL);
	
	timer.it_interval.tv_usec = (INTERVAL*1000)%1000000;
    timer.it_interval.tv_sec = INTERVAL/1000;
    timer.it_value.tv_usec = (INTERVAL*1000)%1000000;
    timer.it_value.tv_sec = INTERVAL/1000;
	
	initialized = 1;
	setitimer(ITIMER_PROF, &timer, NULL);
	
	current = queue_L1;					// Set current = first queue element
	queue_L1 = queue_L1->next;			// remove current from queue
	setcontext(&(current->ctx));			// Start running current
}

//TESTING
/*
void* test_func(int a){
	puts("IN TEST FUNC 1:");
	int i;
	for(i = 0; i < 10; i++){
		printf("%d\n", i);
	}
	rpthread_exit((void * ) 8);
}

void* test_func2(){
	puts("before join");
	rpthread_join(3, (void *) 9);
	puts("after join");
}*/

void* test_func()
{
	while(1)
	{
		puts("F1 (yield)");
		rpthread_yield();
	}
}

void* test_func2()
{
	while(1)
	{
		puts("F2 (no yield)");
	}
}


int main()
{
	rpthread_t t_1 = 10;
	rpthread_t t_2 = 3;
	rpthread_create(&t_1, 0, (void *) test_func2, NULL);
	rpthread_create(&t_2, 0, (void *) test_func, NULL);
	//setUp();
	puts("POST SETUP");
	//schedule();
	
	while(1);
	return 0;
}