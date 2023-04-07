int file_create(int file_type);
int file_write(int inum, int offset, int length, void* buffer);
int file_read(int inum, int offset, int length, void* buffer);
block_address** write_buffer_to_appropriate_blocks(block_address **block_addresses, int number_blocks_affected, void *buffer, int offset, int length,int inum);
block_address** get_affected_blocks_addresses(int inum, int offset, int length, int *number_blocks_affected);
block_address get_block_address(int inum, int block_index);
i_node* get_inode(int inum);
int update_inode_after_write(int inum, int first_block_index, int number_blocks_affected, block_address **new_block_addresses, int eof_index);
int read_from_blocks_to_buffer(block_address **block_addresses, int number_blocks_affected, int offset, int length, void* buffer);
int file_read_begining_to_end(int inum, void* buffer);
int get_file_type(int inum);
int get_file_size(int inum);
int file_free(int inum);
