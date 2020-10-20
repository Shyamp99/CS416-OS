#include "rpthread.h"

void test(){
	puts("int test function\n");
	//return 1 + 2*4;
}

void* swap_test(ucontext_t* nctx){
	/*char buffer [2];
	to_string(*a+*a);
	cout << "the sum is " << buffer;*/
	puts("before setconext");
	setcontext(nctx);
	puts("set context to ncctx\n");
}

int main(int argc, char **argv){
	
	//creating stack
	//stack <int> t_stack;
	void* t_stack = malloc(STACK_SIZE);

	if (t_stack == NULL){
		puts("stack is null \n");
		exit(1);
	}

	//setup context vars
	cctx.uc_link = NULL;
	cctx.uc_stack.ss_sp = t_stack;
	cctx.uc_stack.ss_size=STACK_SIZE;
	cctx.uc_stack.ss_flags=0;

	puts("calling make context\n");

	//setting up context via makecontext
	if (getcontext(&cctx) == -1){
		puts("ruh roh\n");
	}
	makecontext(&cctx, (void*)&swap_test, 1, &ncctx);
	puts("made context\n");

	/*if (getcontext(&cctx) == -1){
		cout << "err making context\n";
	}*/
	
	//set curr context to run the context that runs fuck_bryan
	// puts("before set context\n");
	//setcontext(&cctx);
	// puts("seg fault test\n");

	if (getcontext(&ncctx) == -1){
		exit(1);
	}

	/*t_stack = malloc(STACK_SIZE);

	//setup context vars
	ncctx.uc_link = NULL;
	ncctx.uc_stack.ss_sp = t_stack;
	ncctx.uc_stack.ss_size=STACK_SIZE;
	ncctx.uc_stack.ss_flags=0;

	int temp = 1;
	int temp2 = 2;
	makecontext(&ncctx, (void *)&swap_test, 1, &ncctx);
	*/
	swapcontext(&ncctx, &cctx);
	puts("we hit the end");
	return 0;
}
