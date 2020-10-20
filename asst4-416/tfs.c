/*
 *  Copyright (C) 2020 CS416 Rutgers CS
 *	Tiny File System
 *	File:	tfs.c
 *
 */

#define FUSE_USE_VERSION 26

#define MIN(a,b) \
    ({ __typeof__ (a) _a = (a); \
	       __typeof__ (b) _b = (b); \
		        _a < _b ? _a : _b; })

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];
int curr_blocknum;
struct superblock* super_block;
struct inode root;
bitmap_t* inode_bitmap;
bitmap_t* block_bitmap;

//dirents per block
int dpb = BLOCK_SIZE / sizeof(struct dirent);
int global_temp = -1;

// inodes per block
int ipb = BLOCK_SIZE / sizeof(struct inode);

// number of inode blocks
int num_iblk = MAX_INUM / BLOCK_SIZE;

// inode* curr_inode;
// dirent* curr_dirent;


// Declare your in-memory data structures here

/*
 * Get available inode number from bitmap
 */

int get_avail_ino() {

	// Step 1: Read inode bitmap from disk	
	// Step 2: Traverse inode bitmap to find an available slot
	// Step 3: Update inode bitmap and write to disk 
	/*bitmap_t* bitmap = malloc(sizeof(BLOCK_SIZE));
	if (bio_read(super_block->i_bitmap_blk, bitmap) < 0){
		perror("Unable to read inode bitmap");
		return -1;
	}*/
	int i;
	for (i = 0; i < BLOCK_SIZE; i++) {
		if (inode_bitmap[i]) {
			set_bitmap(inode_bitmap, i);
			// inode_bitmap[i] = !inode_bitmap[i]
			return i;
		}
	}

	return -1;
}

/*
 * Get available data block number from bitmap
 */

int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	// Step 2: Traverse data block bitmap to find an available slot
	// Step 3: Update data block bitmap and write to disk 

	/*bitmap_t* bitmap = malloc(sizeof(BLOCK_SIZE));
	if (bio_read(super_block->d_bitmap_blk, bitmap) < 0){
		perror("Unable to read inode bitmap");
		return -1;
	}*/
	int i;
	for (i = 0; i < BLOCK_SIZE; i++) {
		if (block_bitmap[i]) {
			set_bitmap(block_bitmap, i);
			// block_bitmap[i] = !block_bitmap[i];
			return i;
		}
	}

	return -1;
}

/*
 * inode operations
 */
int readi(uint16_t ino, struct inode* inode) {

	// Step 1: Get the inode's on-disk block number
	// Step 2: Get offset of the inode in the inode on-disk block
	int blockstart = super_block->i_start_blk + ino / 16;  //should autotruncate it, i think
	int no_in_block = ino % 16;

	// Step 3: Read the block from disk and then copy into inode structure
	char* block = malloc(BLOCK_SIZE);
	if (bio_read(blockstart, block) < 0) {
		return -1;
	}
	memcpy(inode, block[no_in_block * sizeof(struct inode)], sizeof(struct inode));

	return 0;
}

int writei(uint16_t ino, struct inode* inode) {

	// Step 1: Get the block number where this inode resides on disk
	// Step 2: Get the offset in the block where this inode resides on disk	
	int blockstart = super_block->i_start_blk + ino / 16;
	int no_in_block = ino % 16;

	// Step 3: Write inode to disk 
	char* block = malloc(BLOCK_SIZE);
	if (bio_read(blockstart, block) < 0) {
		return -1;
	}
	memcpy(block[no_in_block * sizeof(struct inode)], inode, sizeof(struct inode));
	bio_write(blockstart, (void*)block);

	return 0;
}

/*
 * directory operations
 */
int dir_find(uint16_t ino, const char* fname, size_t name_len, struct dirent* dirent) {

	// Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode* directory_inode;
	if (readi(ino, directory_inode) < 0) {
		perror("Reading Block Failed");
		return -1;
	}

	struct dirent dirents[BLOCK_SIZE / sizeof(struct dirent)];

	// Step 2: Get data block of current directory from inode
	// Step 3: Read directory's data block and check each directory entry.
	struct dirent* curr_dirent = malloc(sizeof(struct dirent));
	int block_no, i;
	for (block_no = 0; block_no < 16; block_no++) {
		bio_read(directory_inode->direct_ptr[block_no] + super_block->d_start_blk, dirents);
		global_temp = block_no;
		//if this doesn't work change to i++, brute force [0, block_size) should work in theory
		//dpb is a global for dirents per block
		for (i = 0; i < BLOCK_SIZE; i += sizeof(struct dirent))
			// memcpy(curr_dirent, dirents[i], sizeof(struct dirent));
			//not sure if this line below works, we shall see
			curr_dirent = &dirents[i];
		//If the name matches, then copy directory entry to dirent structure
		if (strcmp(curr_dirent->name, fname)) {
			// memcpy(direct, curr_dirent, sizeof(struct direct));
			dirent = curr_dirent;
			return 0;
		}
	}
	global_temp = -1;
	return -1;
}

int get_avail_direct_ptr(struct inode* inode) {
	int i;
	for (i = 0; i < 16; i++) {
		if (inode->direct_ptr[i] == -1) {
			return i;
		}
	}
	return -1;
}

//need to go over def doesn't work w newfound logic
int dir_add(struct inode dir_inode, uint16_t f_ino, const char* fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode	
	// Step 2: Check if fname (directory name) is already used in other entries
	// Step 3: Add directory entry in dir_inode's data block and write to disk

	if (dir_inode.valid) {
		perror("invalid directory");
		return -1;
	}

	// Allocate a new data block for this directory if it does not exist
	struct dirent* dirent = malloc(sizeof(struct dirent));
	if (dir_find(f_ino, fname, name_len, dirent)) {
		return -1;
	}
	int ptr = get_avail_direct_ptr(&dir_inode);
	if (ptr == -1) {
		perror("no space available in dir_inode");
		return -1;
	}

	dir_inode.direct_ptr[ptr] = get_avail_blkno();
	dir_inode.size += sizeof(struct dirent);
	int blockstart = dir_inode.direct_ptr[ptr] * sizeof(struct dirent) + super_block->d_start_blk;

	strcpy(dirent->name, fname);
	dirent->len = name_len;
	dirent->valid = 1;
	dirent->ino = f_ino;

	// ignore this it's prolly not needed
	// if(create_inode(f_ino) != 0){
	// 	perror("unable to make inode for new directory entry");
	// 	return -1;
	// }

	//get the whole block using bioread
	char* buffer = malloc(BLOCK_SIZE);
	bio_read(blockstart, buffer);

	//add dirent into buffer and then write
	memcpy(buffer[blockstart], dirent, sizeof(struct dirent));
	bio_write(blockstart, buffer);
	writei(f_ino, &dir_inode);

	return 0;
}


int dir_remove(struct inode dir_inode, const char* fname, size_t name_len) {
	if (dir_inode.valid) {
		perror("invalid directory");
		return -1;
	}
	// Step 1: Check if fname exist
	struct dirent* dirent = malloc(sizeof(struct dirent));
	if (dir_find(dir_inode.ino, fname, name_len, dirent) == -1) {
		perror("There is no file of that name in this directory");
		return -1;
	}

	dir_inode.size -= sizeof(dirent);
	int inode = dirent->ino;
	int data_block_no = global_temp;

	//unset the bitmap

	unset_bitmap(inode_bitmap, inode);
	unset_bitmap(block_bitmap, data_block_no);

	//update the inode
	writei(dir_inode.ino, &dir_inode);
	return 0;
}

/*
 * namei operation
 */
int get_node_by_path(const char* path, uint16_t ino, struct inode* inode) {

	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	//need to add checks

	// struct inode* root = super_block->i_start_blk[sizeof(struct inode)*ino-1];

	//each part of the path and calling dir_find on it
	char* tok = strtok(path, '/');
	struct dirent* curr_dirent = malloc(sizeof(struct dirent));
	struct inode* curr_inode = malloc(sizeof(struct inode));
	uint16_t curr_ino = ino;
	while (tok != NULL) {
		tok = strtok(NULL, '/');
		if (tok == NULL) {
			break;
		}
		if (dir_find(curr_ino, tok, strlen(tok), curr_dirent) != 0) {
			return -1;
		}
		//need to check validibity of it but not sure cuz it's uint and unsigned char
		if (curr_dirent == NULL || curr_dirent->valid) {
			perror("invalid path");
			return -1;
		}
		curr_ino = curr_dirent->ino;
	}

	//reading in the node to the struct inode* inode
	readi(curr_ino, inode);

	return 0;
}

/*
 * Make file system
 */
int tfs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	super_block = malloc(sizeof(super_block));
	super_block->magic_num = MAGIC_NUM;
	super_block->max_inum = MAX_INUM;
	super_block->max_dnum = MAX_DNUM;

	super_block->i_bitmap_blk = 1;
	super_block->d_bitmap_blk = 2;
	super_block->i_start_blk = super_block->d_bitmap_blk + ceil((sizeof(block_bitmap) / BLOCK_SIZE));
	super_block->d_start_blk = super_block->i_start_blk + num_iblk;

	// initialize inode bitmap and block bitmap
	inode_bitmap = malloc(sizeof(bitmap_t) * MAX_INUM);
	block_bitmap = malloc(sizeof(bitmap_t) * MAX_DNUM);

	// update bitmap information for root directory

	// update inode for root directory
	root.ino = get_avail_ino();
	//set_bitmap(inode_bitmap, root.ino); //need this?
	root.valid = 1;
	root.size = sizeof(struct dirent);
	root.type = S_IFDIR | 0777;
	root.link = 1;

	int r;
	int bno = get_avail_blkno();
	root.direct_ptr[0] = bno;
	for (r = 1; r < 16; r++)
	{
		root.direct_ptr[r] = -1;
	}
	for (r = 0; r < 8; r++)
	{
		root.indirect_ptr[r] = -1;
	}

	writei(root.ino, &root);
	bio_write(0, &super_block);

	//this part might be redundant
	/*
	root.vstat.st_ino = root.ino;
	root.vstat.st_size = root.size;
	root.vstat.st_mode = root.type;
	root.vstat.st_nlink = root.link;*/

	return 0;
}


/*
 * FUSE file operations
 */
static void* tfs_init(struct fuse_conn_info* conn) {

	// Step 1a: If disk file is not found, call mkfs

	// Step 1b: If disk file is found, just initialize in-memory data structures
	// and read superblock from disk

	printf("helo\n");
	// haven't checked if this works yet
	int ibytes = MAX_INUM / 8;
	int dbytes = MAX_DNUM / 8;

	int fd = dev_open(diskfile_path);
	if (fd)
	{
		tfs_mkfs();
		fd = dev_open(diskfile_path);
	}
	else
	{
		block_bitmap = malloc(sizeof(unsigned char) * dbytes);
		inode_bitmap = malloc(sizeof(unsigned char) * ibytes);

		char* temp_buffer;
		bio_read(0, temp_buffer);
		memcpy((char*)super_block, temp_buffer, sizeof(struct superblock));

		char buffer[BLOCK_SIZE];
		bio_read(0, buffer);
	}
}

static void tfs_destroy(void* userdata) {

	// Step 1: De-allocate in-memory data structures
	char* buffer;
	bio_write(0, (void*)super_block);
	free(super_block);
	free(&root);

	bio_write(1, inode_bitmap);
	bio_write(2, block_bitmap);
	//here until we replace
	free(inode_bitmap);
	free(block_bitmap);
	free(&root);
	free(super_block);
	// Step 2: Close diskfile
	dev_close();
}

static int tfs_getattr(const char* path, struct stat* stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode
	struct inode inode;
	if (get_node_by_path(path, root.ino, &inode) == -1)
	{
		return -1;
	}
	else
	{
		// not sure if these are all the attributes to do
		//stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_mode = inode.vstat.st_mode;
		//stbuf->st_nlink = 2;
		stbuf->st_nlink = inode.link;
		stbuf->st_size = inode.size;
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
		time(&stbuf->st_mtime);
	}

	return 0;
}

static int tfs_opendir(const char* path, struct fuse_file_info* fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	// Step 2: If not find, return -1

	struct inode inode;
	if (get_node_by_path(path, root.ino, &inode) == -1)
	{
		return -1;
	}

	return 0;
}

static int tfs_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	/*1*/
	struct inode inode;
	if (get_node_by_path(path, root.ino, &inode) == -1)
	{
		return -1;
	}

	/*2*/ //maybe works?
	int i = 0;
	char* file_name = basename(path);

	for (i = 0; i < 16; i++)
	{
		if (inode.direct_ptr[i] != -1)
		{
			bio_read(inode.direct_ptr[i], buffer);
			filler(buffer, file_name, NULL, 0);
		}
	}

	return 0;
}


static int tfs_mkdir(const char* path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk

	/*1*/
	char* dir_path = dirname(path);
	char* dir_name = basename(path);

	/*2*/
	struct inode p_inode;
	int temp = get_node_by_path(dir_path, root.ino, &p_inode);
	if (temp) {
		perror("directory already exists");
		return -1;
	}
	/*3*/
	int ino = get_avail_ino();

	/*4*/
	if (dir_add(p_inode, ino, dir_name, strlen(dir_name)) == -1)
	{
		return -1;
	}
	/*5*/
	struct inode t_inode;
	t_inode.ino = ino;
	t_inode.valid = 1;
	t_inode.size = sizeof(struct dirent) * 2;
	t_inode.type = mode;
	t_inode.link = 1;

	int r;
	t_inode.direct_ptr[0] = get_avail_blkno();
	for (r = 1; r < 16; r++)
	{
		t_inode.direct_ptr[r] = -1;
	}
	for (r = 0; r < 8; r++)
	{
		t_inode.indirect_ptr[r] = -1;
	}
	t_inode.vstat.st_mode = mode;
	// needs more?

	/*6*/
	writei(ino, &t_inode);
	return 0;
}

static int tfs_rmdir(const char* path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	/*1*/
	char* dir_path = dirname(path);
	char* dir_name = basename(path);

	/*2*/
	struct inode t_inode;
	if (get_node_by_path(path, root.ino, &t_inode) == -1)
	{
		return -1;
	}

	/*3*/
	struct inode p_inode;
	if (get_node_by_path(dir_path, root.ino, &p_inode) == -1)
	{
		return -1;
	}
	int i;
	char buff[BLOCK_SIZE];
	struct dirent* dirent = malloc(sizeof(struct dirent));
	for (i = 0; i < 16; i++)
	{
		bio_read(p_inode.direct_ptr[i], buff);
		memcpy(dirent, buff, sizeof(struct dirent));
		if (dirent->name == basename(dir_path))
		{
			unset_bitmap(block_bitmap, p_inode.direct_ptr[i]);
			p_inode.direct_ptr[i] = -1;
			break;
		}
	}

	/*4*/
	unset_bitmap(inode_bitmap, t_inode.ino);
	inode_bitmap[t_inode.ino] = -1;

	/*5*/
	/*
	struct inode p_inode;
	if (get_node_by_path(dir_path, root.ino, &p_inode) == -1)
	{
		return -1;
	}*/

	/*6*/
	dir_remove(p_inode, dir_name, strlen(dir_name));

	return 0;
}

static int tfs_releasedir(const char* path, struct fuse_file_info* fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	/*1*/
	char* dir_path = dirname(path);
	char* f_name = basename(path);

	/*2*/
	struct inode p_inode;
	if (get_node_by_path(dir_path, root.ino, &p_inode) == -1)
	{
		return -1;
	}

	/*3*/
	int ino = get_avail_ino();

	/*4*/
	if (dir_add(p_inode, ino, f_name, strlen(f_name)) == -1)
	{
		return -1;
	}

	/*5*/
	struct inode t_inode;
	t_inode.ino = ino;
	t_inode.valid = 1;
	t_inode.size = 0;
	t_inode.type = mode;
	t_inode.link = 1;

	int r;
	for (r = 0; r < 16; r++)
	{
		t_inode.direct_ptr[r] = -1;
	}
	for (r = 0; r < 8; r++)
	{
		t_inode.indirect_ptr[r] = -1;
	}

	t_inode.vstat.st_mode = mode;
	// needs more?

	/*6*/
	writei(ino, &t_inode);

	return 0;
}

static int tfs_open(const char* path, struct fuse_file_info* fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	struct inode inode;
	if (get_node_by_path(path, root.ino, &inode) == -1)
	{
		return -1;
	}

	return inode.ino;
}

static int tfs_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer

	int bytes_read = 0;
	int sizeleft = size;

	/*1*/
	struct inode inode;
	if (get_node_by_path(path, root.ino, &inode) == -1)
	{
		return -1;
	}

	/*2 & 3*/
	char block_buff[BLOCK_SIZE];
	int block_index = offset / BLOCK_SIZE;
	int block_offset = offset % BLOCK_SIZE;
	int numblocks = (int)ceil((double)size / (double)BLOCK_SIZE);
	if (numblocks > 16) { numblocks = 16; }

	int i;
	for (i = block_index; i < block_index + numblocks; i++)
	{
		if (inode.direct_ptr[i] != -1)
		{
			bio_read(inode.direct_ptr[i], block_buff);

			int copy_amt = MIN(sizeleft - block_offset, BLOCK_SIZE - block_offset);
			memcpy(buffer, block_buff + block_offset, copy_amt);

			block_offset = 0;
			sizeleft -= copy_amt;
			buffer += copy_amt;
			bytes_read += copy_amt;

			if (sizeleft < 0) { sizeleft = 0; }
		}
		else
		{
			perror("Block cannot be read");
		}
	}

	return bytes_read;
}

static int tfs_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk

	int bytes_wrote = 0;
	int sizeleft = size;

	/*1*/ //as usual
	struct inode inode;
	if (get_node_by_path(path, root.ino, &inode) == -1)
	{
		return -1;
	}

	char block_buff[BLOCK_SIZE];
	int block_index = offset / BLOCK_SIZE;
	int block_offset = offset % BLOCK_SIZE;
	int numblocks = (int)ceil((double)size / (double)BLOCK_SIZE);
	if (numblocks > 16) { numblocks = 16; }

	int i;
	for (i = block_index; i < block_index + numblocks; i++)
	{
		if (inode.direct_ptr[i] != -1)
		{
			bio_read(inode.direct_ptr[i], block_buff);

			int copy_amt = MIN(sizeleft - block_offset, BLOCK_SIZE - block_offset);
			memcpy(block_buff + block_offset, buffer, copy_amt);

			bio_write(inode.direct_ptr[i], block_buff);

			block_offset = 0;
			sizeleft -= copy_amt;
			buffer += copy_amt;
			bytes_wrote += copy_amt;

			if (sizeleft < 0) { sizeleft = 0; }
		}
		else
		{
			inode.direct_ptr[i] = get_avail_blkno();
			bio_read(inode.direct_ptr[i], block_buff);

			int copy_amt = MIN(sizeleft - block_offset, BLOCK_SIZE - block_offset);
			memcpy(block_buff + block_offset, buffer, copy_amt);

			bio_write(inode.direct_ptr[i], block_buff);

			block_offset = 0;
			sizeleft -= copy_amt;
			buffer += copy_amt;
			bytes_wrote += copy_amt;

			if (sizeleft < 0) { sizeleft = 0; }
			inode.link++;
			inode.size += copy_amt;
		}
	}

	writei(inode.ino, &inode);
	return bytes_wrote;
}

static int tfs_unlink(const char* path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	/*1*/
	char* dir_path = dirname(path);
	char* f_name = basename(path);

	/*2*/
	struct inode f_inode;
	if (get_node_by_path(path, root.ino, &f_inode) == -1)
	{
		return -1;
	}

	/*3*/
	struct inode p_inode;
	if (get_node_by_path(dir_path, root.ino, &p_inode) == -1)
	{
		return -1;
	}
	int i;
	char buff[BLOCK_SIZE];
	struct dirent* dirent = malloc(sizeof(struct dirent));
	for (i = 0; i < 16; i++)
	{
		bio_read(p_inode.direct_ptr[i], buff);
		memcpy(dirent, buff, sizeof(struct dirent));
		if (dirent->name == f_name)
		{
			unset_bitmap(block_bitmap, p_inode.direct_ptr[i]);
			break;
		}
	}

	/*4*/
	inode_bitmap[f_inode.ino] = -1;

	/*5*/
	/* done in part 3
	struct inode p_inode;
	if (get_node_by_path(dir_path, root.ino, &p_inode) == -1)
	{
		return -1;
	}*/

	/*6*/
	dir_remove(p_inode, f_name, strlen(f_name));

	return 0;
}

static int tfs_truncate(const char* path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_release(const char* path, struct fuse_file_info* fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char* path, struct fuse_file_info* fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_utimens(const char* path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}


static struct fuse_operations tfs_ope = {
	.init = tfs_init,
	.destroy = tfs_destroy,

	.getattr = tfs_getattr,
	.readdir = tfs_readdir,
	.opendir = tfs_opendir,
	.releasedir = tfs_releasedir,
	.mkdir = tfs_mkdir,
	.rmdir = tfs_rmdir,

	.create = tfs_create,
	.open = tfs_open,
	.read = tfs_read,
	.write = tfs_write,
	.unlink = tfs_unlink,

	.truncate = tfs_truncate,
	.flush = tfs_flush,
	.utimens = tfs_utimens,
	.release = tfs_release
};


int main(int argc, char* argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}