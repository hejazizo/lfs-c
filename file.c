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


/*
 * Reads data from the file with the given i-num, starting at the given offset and continuing for the given length.
 * Calculates the affected blocks and reads data from those blocks into the buffer.
 *
 * @param inum the i-num of the file to read from
 * @param offset the offset at which to start reading in bytes
 * @param length the length of the data to read in bytes
 * @param buffer the buffer to read the data into
 * @return 0 on success, -1 on error
 */
int read_from_file(int inum, int offset, int length, void *buffer)
{
    // Calculate affected blocks and find their segment and block number for reading
    int num_blocks_affected = 0;
    block_address **block_addresses = get_affected_blocks_addresses(inum, offset, length, &num_blocks_affected);

    // Read data from blocks into buffer
    read_from_blocks_to_buffer(block_addresses, num_blocks_affected, offset, length, buffer);

    // Free memory allocated for block addresses
    for (int i = 0; i < num_blocks_affected; i++) {
        free(block_addresses[i]);
    }
    free(block_addresses);

    // Return 0 on success
    return 0;
}


/*
 * Reads the entire contents of the file with the given i-num from the beginning to the end.
 * Gets the file's inode to determine the size of the file and reads the data into the buffer.
 *
 * @param inum the i-num of the file to read from
 * @param buffer the buffer to read the data into
 * @return 0 on success, -1 on error
 */
int read_file_from_start_to_end(int inum, void *buffer)
{
    // Get the file's inode to determine the size of the file
    i_node *file_inode = get_inode(inum);

    // Read the entire contents of the file into the buffer
    read_from_file(inum, 0, file_inode->eof_index_in_bytes + 1, buffer);

    // Free the inode
    free(file_inode);

    // Return 0 on success
    return 0;
}


/*
 * Frees the file with the given i-num by removing its mapping from the i-file.
 *
 * @param inum the i-num of the file to free
 * @return 0 on success, -1 on error
 */
int free_file(int inum)
{
    // Remove the file's mapping from the i-file
    Log_Remove_mapping(inum);

    // Return 0 on success
    return 0;
}


/**
* Writes the given buffer to the appropriate blocks, updates the inode and writes it to a new location,
* and updates the IFile with the new mapping.
*
* @param block_addresses Array of pointers to block_address structures for the affected blocks.
* @param number_blocks_affected The number of affected blocks.
* @param buffer The buffer to write.
* @param offset The offset in the file to start writing from.
* @param length The length of the data to write.
* @param inum The inode number of the file to write to.
*
* @return The new block addresses after writing to the blocks.
*/
block_address **write_buffer_to_appropriate_blocks(block_address **block_addresses, int number_blocks_affected, void *buffer, int offset, int length, int inum)
{
    // Allocate space to store new block addresses for the INODE to be returned
    block_address **new_block_addresses = (block_address **)calloc(number_blocks_affected, sizeof(block_address *));

    int block_size_in_bytes = sp->block_size_in_sectors * 512;
    int start_block_no = (int)ceil((float)offset / block_size_in_bytes);

    // Divide the buffer into an array of smaller buffers, each to be written to one block
    // void **buffers = (void **)calloc(number_blocks_affected, sizeof(void *));

    for (int i = 0; i < number_blocks_affected; ++i)
    {
        void *tmp_buffer = (void *)calloc(sp->block_size_in_sectors * 512, sizeof(char));
        block_address tmp_address;

        // If a block is affected and already exists, read it into memory and then write to it.
        // If it does not exist, just write to it.
        if (block_addresses[i]->segment_no != -1)
        {
            // Block already exists. Read its content and overwrite it.
            tmp_address.segment_no = block_addresses[i]->segment_no;
            tmp_address.block_no = block_addresses[i]->block_no;

            // Read the existing block
            Log_Read(tmp_address, sp->block_size_in_sectors * 512, tmp_buffer);
        }

        // Determine the appropriate starting index and length for the current block
        int buffer_start_index = -1;
        int tmp_start_index = -1;
        int new_length = -1;

        if (i == 0)
        {
            tmp_start_index = offset % block_size_in_bytes;
            buffer_start_index = 0;
            new_length = (block_size_in_bytes < length) ? block_size_in_bytes - tmp_start_index : length;
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

        // Write to the temporary buffer
        memcpy(tmp_buffer + (tmp_start_index), buffer + (buffer_start_index), (new_length));

        // Write to log
        char *desc = (char *)malloc(31 * sizeof(char));
        sprintf(desc, "Data for inum: %d", inum);

        Log_Write(&tmp_address, inum, start_block_no + i, new_length, tmp_buffer, desc);
        new_block_addresses[i] = (block_address *)calloc(1, sizeof(block_address));
        new_block_addresses[i]->segment_no = tmp_address.segment_no;
        new_block_addresses[i]->block_no = tmp_address.block_no;
    }

    return new_block_addresses;
}

int read_from_blocks_to_buffer(block_address **block_addresses, int number_blocks_affected, int offset, int length, void *buffer)
{
    // Calculate the block size in bytes
    int block_size_in_bytes = sp->block_size_in_sectors * 512;

    // Loop through the affected blocks
    for (int i = 0; i < number_blocks_affected; ++i)
    {
        // Allocate memory for the temporary buffer
        void *tmp_buffer = (void *)calloc(sp->block_size_in_sectors * 512, sizeof(char));

        // Get the address of the current block
        block_address *current_block_address = block_addresses[i];

        // Check if the current block exists
        if (current_block_address->segment_no == -1)
        {
            // If the block doesn't exist, fill it with zeros
            memset(tmp_buffer, 0, block_size_in_bytes);
        }
        else
        {
            // If the block exists, read its contents to the temporary buffer
            Log_Read(*current_block_address, block_size_in_bytes, tmp_buffer);
        }

        // Determine the appropriate starting index and length for the current block
        int buffer_start_index = -1;
        int tmp_start_index = -1;
        int new_length = -1;

        if (i == 0)
        {
            tmp_start_index = offset % block_size_in_bytes;
            buffer_start_index = 0;
            new_length = (block_size_in_bytes < length) ? block_size_in_bytes - tmp_start_index : length;
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

        // Write the data from the temporary buffer to the main buffer
        memcpy(buffer + (buffer_start_index), tmp_buffer + (tmp_start_index), (new_length));

        // Free the memory allocated for the temporary buffer
        free(tmp_buffer);
    }

    return 0;
}


/**
 * Updates the inode information after a write operation.
 *
 * @param inum                  The inode number.
 * @param first_block_index     The index of the first block affected by the write operation.
 * @param number_blocks_affected The number of blocks affected by the write operation.
 * @param new_block_addresses   The new block addresses to update in the inode.
 * @param eof_index             The index of the end of the file after the write operation.
 *
 * @return                      Returns 0 on success.
 */
int update_inode_after_write(int inum, int first_block_index, int number_blocks_affected, block_address **new_block_addresses, int eof_index)
{
	// Get the current inode for the given inode number
	i_node *current_inode = get_inode(inum);

	// Read the indirect block addresses into memory
	block_address indirect_addresses[sp->block_size_in_sectors * 512 / sizeof(block_address)];
	Log_Read(current_inode->indirect_block, sp->block_size_in_sectors * 512, (void *)(&indirect_addresses));

	// Update the block addresses in the inode for each block affected by the write operation
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

	// Write the updated indirect block addresses to a new block
	block_address *new_indirect_block_address = (block_address *)calloc(1, sizeof(block_address));
	char *desc = (char *)malloc(31 * sizeof(char));
	sprintf(desc, "Indirect Block for inum: %d", inum);
	Log_Write(new_indirect_block_address, inum, -1, 0, ((void *)(&indirect_addresses)), desc);

	// Update the inode with the new indirect block address and end of file index
	current_inode->indirect_block.segment_no = new_indirect_block_address->segment_no;
	current_inode->indirect_block.block_no = new_indirect_block_address->block_no;
	current_inode->eof_index_in_bytes = eof_index;

	// Write the updated inode to a new block and update IFILE with the new inode address
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

	// Write the updated IFILE to a new block
	Log_Write_IFile();

	return 0;
}


/**
* Returns the block addresses affected by a write operation.
*
* @param inum                  The inode number.
* @param offset                The offset of the write operation.
* @param length                The length of the write operation.
* @param number_blocks_affected The number of blocks affected by the write operation.
*
* @return                      Returns an array of block addresses affected by the write operation.
*/
block_address **get_affected_blocks_addresses(int inum, int offset, int length, int *number_blocks_affected)
{
	// Calculate the start and end block numbers affected by the write operation
	int block_size_in_bytes = sp->block_size_in_sectors * 512;
	int start_block_no = (int)floor((float)offset / block_size_in_bytes);
	int end_block_no = (int)floor((float)(offset + length - 1) / block_size_in_bytes);

	// Allocate memory for an array of block addresses affected by the write operation
	block_address **addresses = (block_address **)calloc(end_block_no - start_block_no + 1, sizeof(block_address *));

	// Get the block address for each block affected by the write operation
	for (int i = start_block_no; i <= end_block_no; ++i)
	{
		addresses[i] = (block_address *)calloc(1, sizeof(block_address));
		block_address tmp = get_block_address(inum, i);
		addresses[i]->segment_no = tmp.segment_no;
		addresses[i]->block_no = tmp.block_no;
	}

	// Calculate the number of blocks affected by the write operation
	*number_blocks_affected = end_block_no - start_block_no + 1;

	return addresses;
}


/**
 * Returns the block address for a given block index in the inode.
 *
 * @param inum          The inode number.
 * @param block_index   The index of the block.
 *
 * @return              Returns the block address for the given block index in the inode.
 */
block_address get_block_address(int inum, int block_index)
{
	// Initialize the block address
	block_address address;
	address.segment_no = -1;
	address.block_no = -1;

	// Get the inode for the given inode number
	i_node *tmp = get_inode(inum);

	// If the block is a direct block, return the block address from the direct block array
	if (block_index < 4)
	{
		return tmp->direct_block[block_index];
	}
	// If the block is an indirect block, read the indirect block addresses into memory and return the block address from the indirect block array
	else
	{
		block_address indirect_addresses[sp->block_size_in_sectors * 512 / sizeof(block_address)];
		Log_Read(tmp->indirect_block, sp->block_size_in_sectors * 512, (void *)(&indirect_addresses));
		return indirect_addresses[block_index - 4];
	}
}


/**
 * Returns the file type for a given inode number.
 *
 * @param inum          The inode number.
 *
 * @return              Returns the file type for the given inode number.
 */
int get_file_type(int inum)
{
	// Get the inode for the given inode number
	i_node *tmp = get_inode(inum);

	// Return the file type from the inode
	return tmp->file_type;
}


/**
 * Returns the size of the file for a given inode number.
 *
 * @param inum          The inode number.
 *
 * @return              Returns the size of the file for the given inode number.
 */
int get_file_size(int inum)
{
	// Get the inode for the given inode number
	i_node *tmp = get_inode(inum);

	// Return the end of file index plus one from the inode
	return tmp->eof_index_in_bytes + 1;
}


/**
 * Returns the inode for a given inode number.
 *
 * @param inum          The inode number.
 *
 * @return              Returns the inode for the given inode number.
 */
i_node *get_inode(int inum)
{
	// Initialize the inode address
	block_address INode_address;
	INode_address.segment_no = -1;
	INode_address.block_no = -1;

	// Search the IFILE for the inode address associated with the given inode number
	for (int i = 0; i < sp->i_node_mapping_count; ++i)
	{
		if (IFile[i]->i_num == inum)
		{
			INode_address.segment_no = IFile[i]->i_node_address.segment_no;
			INode_address.block_no = IFile[i]->i_node_address.block_no;
		}
	}

	// If the inode address is not found, return an empty inode with an inode number of -1
	if (INode_address.segment_no == -1)
	{
		printf("File Inode not found for %d.\n", inum);
		i_node *tmp = (i_node *)calloc(1, sizeof(i_node));
		tmp->i_num = -1;
		return tmp;
	}

	// Read the inode into memory and return it
	i_node *tmp = (i_node *)calloc(1, sizeof(i_node));
	Log_Read(INode_address, sizeof(i_node), (void *)tmp);
	return tmp;
}
