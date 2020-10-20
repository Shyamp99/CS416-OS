#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

typedef unsigned char valid_bit;

#define TLB_ENTRIES 512

//Structure to represents TLB
struct tlb {
    /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
    * Think about the size of each TLB entry that performs virtual to physical
    * address translation.
    */
    /*LOGIC:
        contains 3 arrays with a 1:1:1 index correspondence:
        1. the entry number in the page directory
        2. the physical mem address
        3. a short in array for entry age (with each mem access we increment, when insert new set to 0)
    */
    int mem_accesses;
    int miss_count;
    int page_dir_nums[TLB_ENTRIES];
    unsigned long physical_addrs[TLB_ENTRIES];
    short int age[TLB_ENTRIES];

};
struct tlb tlb_store;

pde_t* page_dir;

unsigned char* physical_mem;

pthread_mutex_t pt_lock,
vbitmap_lock,
pbitmap_lock,
tlb_lock,
phys_mem_lock;

int vpage_count;
int ppage_count;
int page_entries;
//int page_count = (int)MEMSIZE / PGSIZE;
int ptable_count;


valid_bit* vbitmap;
valid_bit* pbitmap;

void set_physical_mem();
unsigned int get_top_bits(unsigned int value, int num_bits);
unsigned int get_end_bits(unsigned int value, int num_bits);
unsigned int get_mid_bits(unsigned int value, int num_middle_bits, int num_lower_bits);
int get_ppn(void*);
pte_t* translate(pde_t* pgdir, void* va);
int page_map(pde_t* pgdir, void* va, void* pa);
bool check_in_tlb(void* va);
void put_in_tlb(void* va, void* pa);
void* a_malloc(unsigned int num_bytes);
void a_free(void* va, int size);
void put_value(void* va, void* val, int size);
void get_value(void* va, void* val, int size);
void mat_mult(void* mat1, void* mat2, int size, void* answer);
void print_TLB_missrate();

#endif