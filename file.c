#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include <time.h>
#include "file.h"
#include <math.h>
#include <string.h>

/*
 * Creates a new file of the given file_type and returns the newly generated i-num.
 * Initializes the file's inode and saves it in a block using the Log_Write function.
 * Also updates the i-file and super segment.
 *
 * @param file_type the type of the file to create (0 for regular file, 1 for directory)
 * @return the newly generated i-num for the created file
 */
int create_file(int file_type)
{
    // Generate a new i-num
    int inum = sp->next_inum;
    (sp->next_inum)++;

    // Create a new inode for the file
    i_node file_inode;
    file_inode.i_num = inum;
    file_inode.file_type = file_type;
    file_inode.file_size_in_blocks = 0;
    file_inode.eof_index_in_bytes = 0;

    // Initialize the 4 direct data block addresses to null and the file size to zero.
    // When writing to the file, check if there is enough space available and initialize new blocks if necessary.
    block_address null_block = {-1, -1};
    file_inode.direct_block[0] = null_block;
    file_inode.direct_block[1] = null_block;
    file_inode.direct_block[2] = null_block;
    file_inode.direct_block[3] = null_block;

    // Allocate space for the indirect block and initialize its addresses to null
    block_address *indirect = (block_address *)calloc(1, sizeof(block_address));
    block_address indirect_addresses[sp->block_size_in_sectors * 512 / sizeof(block_address)];
    for (int i = 0; i < sp->block_size_in_sectors * 512 / sizeof(block_address); ++i)
    {
        indirect_addresses[i] = null_block;
    }

    // Write the indirect block to the log
    char *desc = (char *)malloc(31 * sizeof(char));
    sprintf(desc, "Indirect Block for inum: %d", inum);
    Log_Write(indirect, inum, -1, sizeof(indirect_addresses), (void *)(&indirect_addresses), desc);

    // Set the indirect block address in the inode
    file_inode.indirect_block.segment_no = indirect->segment_no;
    file_inode.indirect_block.block_no = indirect->block_no;

    // Set the timestamps for the inode to the current time
    time_t now = time(NULL);
    file_inode.modify_Time = now;
    file_inode.access_Time = now;
    file_inode.create_Time = now;
    file_inode.change_Time = now;

    // Save the inode in a block using the Log_Write function
    block_address *address = (block_address *)calloc(1, sizeof(block_address));
    desc = (char *)malloc(31 * sizeof(char));
    sprintf(desc, "Inode for inum: %d", inum);
    Log_Write(address, inum, -1, 0, ((void *)&file_inode), desc);

    // Update the i-file and the super segment
    i_node_block_address_mapping new_file_mapping;
    new_file_mapping.i_num = inum;
    new_file_mapping.i_node_address = *address;
    Log_Add_mapping(new_file_mapping);

    // Return the newly generated i-num for the created file
    return inum;
}


/*
 * Writes the given buffer to the file with the given i-num, starting at the given offset and continuing for the given length.
 * Calculates the affected blocks and writes the buffer to those blocks using the write_buffer_to_appropriate_blocks function.
 * Reads and updates the file's inode, writes it to the new location, and updates the i-file with the new mapping.
 *
 * @param inum the i-num of the file to write to
 * @param offset the offset at which to start writing in bytes
 * @param length the length of the data to write in bytes
 * @param buffer the buffer containing the data to write
 * @return 0 on success, -1 on error
 */
int write_to_file(int inum, int offset, int length, void *buffer)
{
    // Calculate affected blocks and find their segment and block number for changes in the inode
    int num_blocks_affected = 0;
    block_address **block_addresses = get_affected_blocks_addresses(inum, offset, length, &num_blocks_affected);

    // Write the buffer to the appropriate blocks
    block_address **new_block_addresses = write_buffer_to_appropriate_blocks(block_addresses, num_blocks_affected, buffer, offset, length, inum);

    // Update the inode after writing the buffer to the blocks
    int block_size_in_bytes = sp->block_size_in_sectors * 512;
    int first_affected_block_index = (int)floor((float)offset / block_size_in_bytes);
    update_inode_after_write(inum, first_affected_block_index, num_blocks_affected, new_block_addresses, offset + length - 1);

    // Free memory allocated for block addresses
    for (int i = 0; i < num_blocks_affected; i++) {
        free(block_addresses[i]);
    }
    free(block_addresses);

    for (int i = 0; i < num_blocks_affected; i++) {
        free(new_block_addresses[i]);
    }
    free(new_block_addresses);

    // Return 0 on success
    return 0;
}


int file_read(int inum, int offset, int length, void *buffer)
{
	// calculate affected blocks and find their segment and block no for reading
	int *number_blocks_affected = (int *)calloc(1, sizeof(int));
	block_address **block_addresses = get_affected_blocks_addresses(inum, offset, length, number_blocks_affected);
	read_from_blocks_to_buffer(block_addresses, *number_blocks_affected, offset, length, buffer);

	return 0;
}

int file_read_begining_to_end(int inum, void *buffer)
{
	i_node *tmp = get_inode(inum);

	file_read(inum, 0, tmp->eof_index_in_bytes + 1, buffer);

	return 0;
}

int file_free(int inum)
{
	Log_Remove_mapping(inum);

	return 0;
}

block_address **write_buffer_to_appropriate_blocks(block_address **block_addresses, int number_blocks_affected, void *buffer, int offset, int length, int inum)
{
	// a space to keep the new written block addresses for INODE (this value is returned back)
	block_address **new_block_addresses = (block_address **)calloc(number_blocks_affected, sizeof(block_address *));
	// printf("number blocks affected: %d\n",number_blocks_affected );

	int block_size_in_bytes = sp->block_size_in_sectors * 512;
	int start_block_no = (int)ceil((float)offset / block_size_in_bytes);

	// devide the buffer into an array of buffers each of which is written to one block
	void **buffers = (void **)calloc(number_blocks_affected, sizeof(void *));

	for (int i = 0; i < number_blocks_affected; ++i)
	{
		void *tmp_buffer = (void *)calloc(sp->block_size_in_sectors * 512, sizeof(char));
		block_address tmp_address;

		// if a block is affected and already exists, first need to read it to memory and then write to it.
		// if not exist, just write to it.
		if (block_addresses[i]->segment_no != -1)
		{
			// block already exists. read its content and overwrite it
			tmp_address.segment_no = block_addresses[i]->segment_no;
			tmp_address.block_no = block_addresses[i]->block_no;

			// read the existing block
			Log_Read(tmp_address, sp->block_size_in_sectors * 512, tmp_buffer);
		}

		// find the appropriate starting index and length
		int buffer_start_index = -1;
		int tmp_start_index = -1;
		int new_length = -1;

		if (i == 0)
		{
			tmp_start_index = offset % block_size_in_bytes;
			buffer_start_index = 0;
			if (block_size_in_bytes < length)
			{
				new_length = block_size_in_bytes - tmp_start_index;
			}
			else
			{
				new_length = length;
			}
		}
		else if (i == number_blocks_affected - 1)
		{
			tmp_start_index = 0;
			new_length = (offset + length - 1) % block_size_in_bytes + 1;
			buffer_start_index = offset + length - new_length;
		}
		else
		{
			tmp_start_index = 0;
			new_length = block_size_in_bytes;
			buffer_start_index = (i - 1) * (block_size_in_bytes) + (block_size_in_bytes - offset % block_size_in_bytes);
		}

		// printf("tmp: write block %d from %d to %d.\n", i,tmp_start_index,tmp_start_index+new_length-1);
		// printf("buffer: write block %d from %d to %d.\n", i,buffer_start_index,buffer_start_index+new_length-1);

		// write to the tmp buffer
		memcpy(tmp_buffer + (tmp_start_index), buffer + (buffer_start_index), (new_length));
		// printf("%s\n", (char *)tmp_buffer);
		// write to log

		char *desc = (char *)malloc(31 * sizeof(char));
		sprintf(desc, "Data for inum: %d", inum);

		Log_Write(&tmp_address, inum, start_block_no + i, new_length, tmp_buffer, desc);
		new_block_addresses[i] = (block_address *)calloc(1, sizeof(block_address));
		new_block_addresses[i]->segment_no = tmp_address.segment_no;
		new_block_addresses[i]->block_no = tmp_address.block_no;
		// printf("new address for block %d: <%d,%d>\n",i, tmp_address.segment_no,tmp_address.block_no);
	}

	return new_block_addresses;
}

int read_from_blocks_to_buffer(block_address **block_addresses, int number_blocks_affected, int offset, int length, void *buffer)
{
	int block_size_in_bytes = sp->block_size_in_sectors * 512;

	for (int i = 0; i < number_blocks_affected; ++i)
	{
		void *tmp_buffer = (void *)calloc(sp->block_size_in_sectors * 512, sizeof(char));
		block_address tmp_address;

		tmp_address.segment_no = block_addresses[i]->segment_no;
		tmp_address.block_no = block_addresses[i]->block_no;

		Log_Read(tmp_address, sp->block_size_in_sectors * 512, tmp_buffer);

		// find the appropriate starting index and length
		int buffer_start_index = -1;
		int tmp_start_index = -1;
		int new_length = -1;

		if (i == 0)
		{
			tmp_start_index = offset % block_size_in_bytes;
			buffer_start_index = 0;
			if (block_size_in_bytes < length)
			{
				new_length = block_size_in_bytes - tmp_start_index;
			}
			else
			{
				new_length = length;
			}
		}
		else if (i == number_blocks_affected - 1)
		{
			tmp_start_index = 0;
			new_length = (offset + length - 1) % block_size_in_bytes + 1;
			buffer_start_index = offset + length - new_length;
		}
		else
		{
			tmp_start_index = 0;
			new_length = block_size_in_bytes;
			buffer_start_index = (i - 1) * (block_size_in_bytes) + (block_size_in_bytes - offset % block_size_in_bytes);
		}

		// write to the buffer
		memcpy(buffer + (buffer_start_index), tmp_buffer + (tmp_start_index), (new_length));
		// printf("Read From %d to %d. \n", tmp_start_index,tmp_start_index+new_length-1);
	}

	return 0;
}

int update_inode_after_write(int inum, int first_block_index, int number_blocks_affected, block_address **new_block_addresses, int eof_index)
{
	i_node *current_inode = get_inode(inum);
	block_address indirect_addresses[sp->block_size_in_sectors * 512 / sizeof(block_address)];
	Log_Read(current_inode->indirect_block, sp->block_size_in_sectors * 512, (void *)(&indirect_addresses));

	for (int i = first_block_index; i < first_block_index + number_blocks_affected; ++i)
	{
		if (i < 4)
		{
			current_inode->direct_block[i].segment_no = new_block_addresses[i - first_block_index]->segment_no;
			current_inode->direct_block[i].block_no = new_block_addresses[i - first_block_index]->block_no;
		}
		else
		{
			indirect_addresses[i - 4].segment_no = new_block_addresses[i - first_block_index]->segment_no;
			indirect_addresses[i - 4].block_no = new_block_addresses[i - first_block_index]->block_no;
		}
	}

	block_address *new_indirect_block_address = (block_address *)calloc(1, sizeof(block_address));

	char *desc = (char *)malloc(31 * sizeof(char));
	sprintf(desc, "Indirect Block for inum: %d", inum);

	Log_Write(new_indirect_block_address, inum, -1, 0, ((void *)(&indirect_addresses)), desc);
	current_inode->indirect_block.segment_no = new_indirect_block_address->segment_no;
	current_inode->indirect_block.block_no = new_indirect_block_address->block_no;
	// if (eof_index>current_inode->eof_index_in_bytes)
	// {
	// 	current_inode->eof_index_in_bytes = eof_index;
	// }
	current_inode->eof_index_in_bytes = eof_index;

	// printf("First block address: <%d,%d>\n", new_block_addresses[0]->segment_no,new_block_addresses[0]->block_no);
	// printf("first block index: %d\n", first_block_index);
	// printf("new indirect block address: <%d,%d>\n", new_indirect_block_address->segment_no,new_indirect_block_address->block_no);
	// write INode to new location and update IFILE

	block_address *new_inode_address = (block_address *)calloc(1, sizeof(block_address));

	desc = (char *)malloc(31 * sizeof(char));
	sprintf(desc, "Inode for inum: %d", inum);

	Log_Write(new_inode_address, inum, -1, 0, (void *)current_inode, desc);

	for (int i = 0; i < sp->i_node_mapping_count; ++i)
	{
		if (IFile[i]->i_num == inum)
		{
			IFile[i]->i_node_address.segment_no = new_inode_address->segment_no;
			IFile[i]->i_node_address.block_no = new_inode_address->block_no;
		}
	}

	Log_Write_IFile();

	return 0;
}

block_address **get_affected_blocks_addresses(int inum, int offset, int length, int *number_blocks_affected)
{
	int block_size_in_bytes = sp->block_size_in_sectors * 512;

	int start_block_no = (int)floor((float)offset / block_size_in_bytes);

	int end_block_no = (int)floor((float)(offset + length - 1) / block_size_in_bytes);

	block_address **addresses = (block_address **)calloc(end_block_no - start_block_no + 1, sizeof(block_address *));

	for (int i = start_block_no; i <= end_block_no; ++i)
	{
		addresses[i] = (block_address *)calloc(1, sizeof(block_address));
		block_address tmp = get_block_address(inum, i);
		addresses[i]->segment_no = tmp.segment_no;
		addresses[i]->block_no = tmp.block_no;
	}
	// printf("start block: %d end block: %d\n",start_block_no,end_block_no );
	*number_blocks_affected = end_block_no - start_block_no + 1;

	return addresses;
}

block_address get_block_address(int inum, int block_index)
{
	block_address address;
	address.segment_no = -1;
	address.block_no = -1;

	i_node *tmp = get_inode(inum);

	if (block_index < 4)
	{
		return tmp->direct_block[block_index];
	}
	else
	{
		block_address indirect_addresses[sp->block_size_in_sectors * 512 / sizeof(block_address)];
		Log_Read(tmp->indirect_block, sp->block_size_in_sectors * 512, (void *)(&indirect_addresses));
		// printf("indirect_address %d: <%d,%d>.\n", block_index-4,indirect_addresses[block_index-4].segment_no,indirect_addresses[block_index-4].block_no);
		return indirect_addresses[block_index - 4];
	}
}

int get_file_type(int inum)
{
	i_node *tmp = get_inode(inum);

	return tmp->file_type;
}

int get_file_size(int inum)
{
	i_node *tmp = get_inode(inum);

	return tmp->eof_index_in_bytes + 1;
}

i_node *get_inode(int inum)
{
	block_address INode_address;
	INode_address.segment_no = -1;
	INode_address.block_no = -1;

	i_node *tmp = (i_node *)calloc(1, sizeof(i_node));

	for (int i = 0; i < sp->i_node_mapping_count; ++i)
	{
		if (IFile[i]->i_num == inum)
		{
			INode_address.segment_no = IFile[i]->i_node_address.segment_no;
			INode_address.block_no = IFile[i]->i_node_address.block_no;
		}
	}

	if (INode_address.segment_no == -1)
	{
		printf("File Inode not found for %d.\n", inum);
		tmp->i_num = -1;
		return tmp;
	}

	Log_Read(INode_address, sizeof(i_node), (void *)tmp);

	return tmp;
}