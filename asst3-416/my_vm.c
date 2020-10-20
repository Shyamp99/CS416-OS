#include "my_vm.h"

int off_bits = 0, mid_bits = 0, front_bits = 0;
int num_entries_per_page = PGSIZE / sizeof(pte_t);
/*
Function responsible for allocating and setting your physical memory
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    double ob = log10(PGSIZE) / log10(2);
    off_bits = (int)ceil(ob);
    mid_bits = (32 - off_bits) / 2;
    front_bits = 32 - mid_bits - off_bits;

    ppage_count = 1 << mid_bits;
    ptable_count = (ppage_count * sizeof(pte_t)) / PGSIZE;
    vpage_count = MAX_MEMSIZE / PGSIZE; //idk this one yet

    physical_mem = (unsigned char*)malloc(MEMSIZE);
    page_dir = (pde_t*)malloc(ptable_count * sizeof(pde_t));
    vbitmap = (valid_bit*)malloc(vpage_count);
    pbitmap = (valid_bit*)malloc(ppage_count);
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    
    //*  TESTING PRINTS
    //printf("Off: %d\nMid: %d\nTop: %d\n\n", off_bits, mid_bits, front_bits);
    //sleep(1);

    //printf("ppage_count: %d\nptable_count: %d\n\n", ppage_count, ptable_count);
    //sleep(1);

    //printf("physical_mem: %lx\npage_dir: %lx\n\n", physical_mem, page_dir);
    //sleep(1);
    //*/

    tlb_store.miss_count = 0;
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void* va, pte_t* pa)
{
    pthread_mutex_lock(&tlb_lock);
    tlb_store.miss_count += 1;

    int i = 0, age = -1, oldest = 0;
    while (tlb_store.physical_addrs[i] != 0)
    {
        if (i >= TLB_ENTRIES) { break; }
        if (tlb_store.age[i] > age)
        {
            age = tlb_store.age[i];
            oldest = i;
        }
        i++;
    }

    if (i >= TLB_ENTRIES)
    {
        //printf("\tNot enough space in TLB. Evicting oldest entry\n");
        i = oldest;
    }

    unsigned int entry_value = get_top_bits((unsigned int)va, front_bits + mid_bits);
    unsigned long pa_value = (unsigned long)pa;

    tlb_store.page_dir_nums[i] = entry_value;
    tlb_store.physical_addrs[i] = pa_value;
    tlb_store.age[i] = 0;
    
    pthread_mutex_unlock(&tlb_lock);

    return 1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t* check_TLB(void* va) {

    /* Part 2: TLB lookup code here */
    pthread_mutex_lock(&tlb_lock);
    tlb_store.mem_accesses += 1;
    unsigned int entry_value = get_top_bits((unsigned int)va, front_bits + mid_bits);

    int i = 0;

    int j;
    for (j = 0; j < TLB_ENTRIES; j++)
    {
        if (tlb_store.physical_addrs[j] != 0)
        {
            tlb_store.age[j] += 1;
        }
    }


    while (tlb_store.page_dir_nums[i] != entry_value)
    {
        i++;
        if (i >= TLB_ENTRIES)
        {
            return NULL;
        }
    }

    tlb_store.age[i] = 0;
    pte_t* pa_value = tlb_store.physical_addrs[i];
    pthread_mutex_unlock(&tlb_lock);

    return pa_value;
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate()
{
    double miss_rate = 0.0;
    /*Part 2 Code here to calculate and print the TLB miss rate*/
    pthread_mutex_lock(&tlb_lock);
    if (tlb_store.mem_accesses != 0)
    {
        miss_rate = (double)tlb_store.miss_count / (double)tlb_store.mem_accesses;
    }
    pthread_mutex_unlock(&tlb_lock);

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t* translate(pde_t* pgdir, void* va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

    // check TLB first
    pte_t* paddr = check_TLB(va);

    if (paddr == NULL)
    {
        unsigned int vaddr = (unsigned int)va;
        unsigned int vpn0 = get_top_bits(vaddr, front_bits);
        unsigned int vpn1 = get_mid_bits(vaddr, mid_bits, off_bits);
        unsigned int off = get_end_bits(vaddr, off_bits);

        //printf("\tTLB miss: translating address...\n");
        //printf("\tvirtual address: %lx\n", vaddr);
        //printf("\tpage directory: %lx\n", pgdir);
        pte_t* outer = pgdir[vpn0];
        //printf("\tpage table addr: %lx\n", outer);
        pte_t* inner = outer[vpn1];
        //printf("\tphysical page addr: %lx\n", inner);
        paddr = &inner[off];
        //printf("\tphysical address: %lx\n", paddr);
        //sleep(1);

        add_TLB(va, paddr);
    }
    else
    {
        //printf("\tTLB hit\n");
        //printf("\tphysical address: %lx\n", paddr);
        //sleep(1);
    }

    return paddr;
}




// EXTRACT BITS HELPER --------------------------------------------------------------------------------------------------
// Example 1 EXTRACTING TOP-BITS (Outer bits)

unsigned int get_top_bits(unsigned int value, int num_bits)
{
    //Assume you would require just the higher order (outer)  bits, 
    //that is first few bits from a number (e.g., virtual address) 
    //So given an  unsigned int value, to extract just the higher order (outer)  ?num_bits?
    int num_bits_to_prune = 32 - num_bits; //32 assuming we are using 32-bit address 
    return (value >> num_bits_to_prune);
}


//Example 2 EXTRACTING BITS FROM THE MIDDLE
//Now to extract some bits from the middle from a 32 bit number, 
//assuming you know the number of lower_bits (for example, offset bits in a virtual address)

unsigned int get_mid_bits(unsigned int value, int num_middle_bits, int num_lower_bits)
{

    //value corresponding to middle order bits we will returning.
    unsigned int mid_bits_value = 0;

    // First you need to remove the lower order bits (e.g. PAGE offset bits). 
    value = value >> num_lower_bits;


    // Next, you need to build a mask to prune the outer bits. How do we build a mask?   

    // Step1: First, take a power of 2 for ?num_middle_bits?  or simply,  a left shift of number 1.  
    // You could try this in your calculator too.
    unsigned int outer_bits_mask = (1 << num_middle_bits);

    // Step 2: Now subtract 1, which would set a total of  ?num_middle_bits?  to 1 
    outer_bits_mask = outer_bits_mask - 1;

    // Now time to get rid of the outer bits too. Because we have already set all the bits corresponding 
    // to middle order bits to 1, simply perform an AND operation. 
    mid_bits_value = value & outer_bits_mask;

    return mid_bits_value;

}

unsigned int get_end_bits(unsigned int value, int num_bits)
{
    return (value % (1 << num_bits));
}

//Example 3
//Function to set a bit at "index"
// bitmap is a region where were store bitmap 
static void set_bit_at_index(char* bitmap, int index)
{
    // We first find the location in the bitmap array where we want to set a bit
    // Because each character can store 8 bits, using the "index", we find which 
    // location in the character array should we set the bit to.
    char* region = ((char*)bitmap) + (index / 8);

    // Now, we cannot just write one bit, but we can only write one character. 
    // So, when we set the bit, we should not distrub other bits. 
    // So, we create a mask and OR with existing values
    char bit = 1 << (index % 8);

    // just set the bit to 1. NOTE: If we want to free a bit (*bitmap_region &= ~bit;)
    *region |= bit;

    return;
}


//Example 3
//Function to get a bit at "index"
static int get_bit_at_index(char* bitmap, int index)
{
    //Same as example 3, get to the location in the character bitmap array
    char* region = ((char*)bitmap) + (index / 8);

    //Create a value mask that we are going to check if bit is set or not
    char bit = 1 << (index % 8);

    return (int)(*region >> (index % 8)) & 0x1;
}
// ---------------------------------------------------------------------------------------------------------------------


void update_pbitmap(int index) {
    pthread_mutex_lock(&pbitmap_lock);
    pbitmap[index] = !pbitmap[index];
    pthread_mutex_unlock(&pbitmap_lock);
}

void update_vbitmap(int index) {
    pthread_mutex_lock(&vbitmap_lock);
    vbitmap[index] = !vbitmap[index];
    pthread_mutex_unlock(&vbitmap_lock);
}

/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(pde_t* pgdir, void* va, void* pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping*/

    pte_t addr = (pte_t)va;
    
    unsigned int vaddr = (unsigned int)va;
    int offset_bits = get_top_bits(vaddr, front_bits);
    int second_bits = get_mid_bits(vaddr, mid_bits, off_bits);

    //insert lock call to make threadsafe
    pthread_mutex_lock(&pt_lock);
    if (!pgdir[offset_bits]) {
        pte_t* page = malloc(PGSIZE / sizeof(pte_t));
        pgdir[offset_bits] = (pte_t * )page;
    }


    ((pte_t*)pgdir[offset_bits])[second_bits] = (pte_t)pa;
    pthread_mutex_unlock(&pt_lock);
    //unlock call

    //printf("\tpage_map outer: %lx\n", pgdir[offset_bits]);
    //printf("\tpage_map inner: %lx\n", ((pte_t*)pgdir[offset_bits])[second_bits]);

    return 0;
}

void* get_next_avail_vp(int numpages) {

    //Use virtual address bitmap to find the next free page

    int index = -1;
    int i = 0;
    // looking for a vpage if mode == 0
    pthread_mutex_lock(&vbitmap_lock);
    for (i = 0; i < vpage_count; i++) {
        if (vbitmap[i] == 0) {
            int h;
            bool avail = true;
            for (h = i; h < i + numpages; h++)
            {
                if (vbitmap[h] != 0)
                {
                    avail = false;
                }
            }
            if (avail)
            {
                index = i;
                int h;
                for (h = i; h < i + numpages; h++)
                {
                    vbitmap[h] = 1;
                }
                
                break;
            }
        }
    }
    pthread_mutex_unlock(&vbitmap_lock);

    if (index > -1)
        return &index;
    else
        return NULL;
}

void* get_next_avail_pp() {
    int index = -1;
    int i = 0;
    pthread_mutex_lock(&pbitmap_lock);
    for (i = 0; i < ppage_count; i++) {
        if (pbitmap[i] == 0) {
            index = i;
            pbitmap[i] = 1;
            break;
        }
    }
    pthread_mutex_unlock(&pbitmap_lock);
    if (index > -1)
        return &index;
    else
        return NULL;
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void* a_malloc(unsigned int num_bytes) {

    /*
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

     /*
      * HINT: If the page directory is not initialized, then initialize the
      * page directory. Next, using get_next_avail(), check if there are free pages. If
      * free pages are available, set the bitmaps and map a new page. Note, you will
      * have to mark which physical pages are used.
      */

      /*
      logic:
      1. if num_byes <= page size: call get next_avail_page and then malloc on the pagedir val
      2. if not, see how many pages are needed
      3. return the physsical mem addr??? (check)
      */

    if (physical_mem == NULL) {
        set_physical_mem();
    }

    //next[0] = vpage number, next[1] = physical page number
    int num_pages = (int)ceil((double)num_bytes / (double)PGSIZE);
    //printf("num_pages: %d\n", num_pages);

    //printf("a_mallocing...\n");

    //getting the available CONSECUTIVE vpage entries
    int* next_vp = get_next_avail_vp(num_pages);
    if (next_vp == NULL) {
        puts("\tERROR: cannot find next vp");
        return NULL;
    }
    int vp = *next_vp;
    //printf("\tnext vp: %d\n", *next_vp);

    //getting the available physical pages (doesn't have to be consecutive)
    /*
    int* next_pp = get_next_avail(1);
    if (next_pp == NULL) {
        puts("\tERROR: cannot find next pp");
        return NULL;
    }
    int pp = *next_pp;*/
    //printf("\tnext pp: %d\n", *next_pp);
    //need to figure out how many entries in a page
    //page_dir[next[0]] = (pde_t*)malloc(PGSIZE);
    
    void* vpointer = NULL;

    // for loop for multiple page mallocs?
    unsigned long finalvaddr;
    int n;
    for (n = 0; n < num_pages; n++)
    {
        int* next_pp = get_next_avail_pp();
        if (next_pp == NULL) {
            puts("\tERROR: cannot find next pp");
            return NULL;
        }
        int pp = *next_pp;
        //printf("\tnext pp: %d\n", *next_pp);

        unsigned int off = 0; // assuming new page per a_malloc
        
        // index of inner page = virtual page number % size of page
        unsigned int vpn1 = (vp+n) % num_entries_per_page;
        //printf("\tvpn1: %d\n", vpn1);
        
        // index of outer page = virtual page number / size of page
        unsigned int vpn0 = (vp+n) / num_entries_per_page;
        //printf("\tvpn0: %d\n", vpn0);
        
        unsigned long vaddr = (vpn0 * (1 << (mid_bits + off_bits))) + (vpn1 * (1 << (off_bits))) + off;

        if (n == 0)
        {
            finalvaddr = vaddr;
        }
        
        vpointer = vaddr;
        //printf("\tvirtual address: %lx\n", vaddr);

        unsigned long paddr = &physical_mem[(pp * PGSIZE)];
        void* ppointer = paddr;
        //printf("\tphysical address: %lx\n", paddr);
        page_map(page_dir, vpointer, ppointer);
    }

    //printf("\tfinal virtual address: %lx\n", finalvaddr);
    vpointer = finalvaddr;

    return vpointer;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void* va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */

    int num_pages = (int)ceil((double)size / (double)PGSIZE);
    unsigned long vaddress = (unsigned long)va;
    unsigned int vpn0 = get_top_bits(vaddress, front_bits);
    unsigned int vpn1 = get_mid_bits(vaddress, mid_bits, off_bits);

    int j;
    for (j = 0; j < num_pages; j ++)
    {
        unsigned long vaddr = (vpn0 * (1 << (mid_bits + off_bits))) + (vpn1 * (1 << (off_bits)));
        //printf("\tvaddr: %lx\n", vaddr);
        //printf("\tvaddress: %lx\n", vaddress);

        pde_t* pa = translate(page_dir, (void*)vaddr);
        unsigned int paddr = (unsigned int)pa;

        int vindex = (vpn0 * PGSIZE) + vpn1;
        int pindex = (paddr - (unsigned int)physical_mem) / PGSIZE;

        vbitmap[vindex] = 0;
        pbitmap[pindex] = 0;
        memset(pa, 0, size);

        unsigned int entry_value = get_top_bits(vaddr, front_bits + mid_bits);
        if (check_TLB(va) != NULL)
        {
            int i = 0;
            while (tlb_store.page_dir_nums[i] != entry_value)
            {
                i++;
                if (i >= TLB_ENTRIES)
                {
                    break;
                }
            }

            tlb_store.page_dir_nums[i] = 0;
            tlb_store.physical_addrs[i] = 0;
            tlb_store.age[i] = 0;

            //printf("\tremoved from TLB\n");
        }

        vpn1++;
        if (vpn1 >= num_entries_per_page)
        {
            vpn1 = 0;
            vpn0++;
        }
    }
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void put_value(void* va, void* val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */

    int num_pages = (int)ceil((double)size / (double)PGSIZE);
    unsigned long vaddress = (unsigned long)va;
    unsigned int vpn0 = get_top_bits(vaddress, front_bits);
    unsigned int vpn1 = get_mid_bits(vaddress, mid_bits, off_bits);

    int size_left = size;

    pthread_mutex_lock(&phys_mem_lock);
    int j;
    for (j = 0; j < num_pages; j++)
    {
        unsigned long vaddr = (vpn0 * (1 << (mid_bits + off_bits))) + (vpn1 * (1 << (off_bits)));
        pde_t* paddr = translate(page_dir, (void*)vaddr);

        int size_cpy = size_left;
        if (size_left > size)
        {
            size_cpy = PGSIZE;
        }

        memcpy(paddr, (val + (j * PGSIZE)), size_cpy);

        size_left -= PGSIZE;

        vpn1++;
        if (vpn1 >= num_entries_per_page)
        {
            vpn1 = 0;
            vpn0++;
        }
    }
    pthread_mutex_unlock(&phys_mem_lock);
}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void* va, void* val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
    int num_pages = (int)ceil((double)size / (double)PGSIZE);
    unsigned long vaddress = (unsigned long)va;
    unsigned int vpn0 = get_top_bits(vaddress, front_bits);
    unsigned int vpn1 = get_mid_bits(vaddress, mid_bits, off_bits);

    int size_left = size;

    pthread_mutex_lock(&phys_mem_lock);
    int j;
    for (j = 0; j < num_pages; j++)
    {
        unsigned long vaddr = (vpn0 * (1 << (mid_bits + off_bits))) + (vpn1 * (1 << (off_bits)));
        pde_t* paddr = translate(page_dir, (void*)vaddr);

        int size_cpy = size_left;
        if (size_left > size)
        {
            size_cpy = PGSIZE;
        }

        memcpy((val + (j*PGSIZE)), paddr, size_cpy);

        size_left -= PGSIZE;

        vpn1++;
        if (vpn1 >= num_entries_per_page)
        {
            vpn1 = 0;
            vpn0++;
        }
    }
    pthread_mutex_unlock(&phys_mem_lock);
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void* mat1, void* mat2, int size, void* answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to
     * getting the values from two matrices, you will perform multiplication and
     * store the result to the "answer array"
     */
    int i, j, k;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            int* val = (int*)translate(page_dir, ((int*)answer) + i * size + j);
            *val = 0;

            for (k = 0; k < size; k++) {
                int* val1 = (int*)translate(page_dir, ((int*)mat1) + i * size + k);
                int* val2 = (int*)translate(page_dir, ((int*)mat2) + i * size + k);

                *val += *val1 * *val2;
            }
        }

    }
}
