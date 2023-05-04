#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "flash.h"
#include "log.h"

// the name of the file that resembels the flash drive
char *flash_drive_name = "tmp_flash";

// The temporary buffer for reading segments
void *temporary_buffer;

// The buffer for writing to the tail segment
void *tail_write_buffer;

// Keeps the changes in the most recent segment
const int maxEntitiesPerSegment = 32;
segment_summary_entity recent_segment_entities[maxEntitiesPerSegment];

// The last written block number in the tail segment
int last_written_block_number = 1;

// caching
int num_segments_cached = 0;
void **cache_segments_ptr;
int *free_segments_idx;

// the function to create a flash file and saves the metadata on that. It is called from mklfs
int log_create(char *new_file_name, int blk_size, int seg_size, int log_file_size, int wear_limit)
{
	// Saving the metadata paramenters in the supersegment global variable
	sp = (supersegment *)calloc(1, sizeof(supersegment));

	sp->block_size_in_sectors = blk_size;
	sp->segment_size_in_blocks = seg_size;
	sp->flash_or_log_size_in_segments = log_file_size;
	sp->wear_limit_for_earased_blocks = wear_limit;
	sp->next_segment_number_in_log = 2;
	sp->next_inum = 1;

	sp->i_node_of_i_file.i_num = 0;
	sp->i_node_of_i_file.file_type = 0;
	sp->i_node_of_i_file.file_size_in_blocks = 0;

	block_address dirct_0;
	dirct_0.segment_no = 1;
	dirct_0.block_no = 1;

	block_address dirct_1;
	dirct_1.segment_no = 1;
	dirct_1.block_no = 2;

	block_address dirct_2;
	dirct_2.segment_no = 1;
	dirct_2.block_no = 3;

	block_address dirct_3;
	dirct_3.segment_no = 1;
	dirct_3.block_no = 4;

	block_address indirect;
	indirect.segment_no = 1;
	indirect.block_no = 5;

	sp->i_node_of_i_file.direct_block[0] = dirct_0;
	sp->i_node_of_i_file.direct_block[1] = dirct_1;
	sp->i_node_of_i_file.direct_block[2] = dirct_2;
	sp->i_node_of_i_file.direct_block[3] = dirct_3;

	sp->i_node_of_i_file.indirect_block = indirect;
	sp->i_node_mapping_count = 0;

	// Creating the flash file (flash size in erase blocks)
	int flash_size = (sp->flash_or_log_size_in_segments * sp->segment_size_in_blocks * sp->block_size_in_sectors) / 16;
	Flash_Create(new_file_name, sp->wear_limit_for_earased_blocks, flash_size);
	// -------- Opening the flash and saving the metadata in the super segment (first segment) --------
	// a space to save the total number of blocks
	int *blocks = (int *)calloc(1, sizeof(int));
	Flash *flash;
	flash = Flash_Open(new_file_name, FLASH_SILENT, blocks);

	int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors;
	int w = Flash_Write(flash, 0, size_segment_in_sectors, (void *)sp);

	printf("Super Segment write operation  result: %d - Error no: %d\n", w, errno);

	printf("FLASH creation completed successfully.\nblock_size_in_sectors = %d\nint segment_size_in_blocks = %d\nint flash_or_log_size_in_segments "
		   "= %d\nint wear_limit_for_earased_blocks = %d\nlast segment number in log = %d\n",
		   sp->block_size_in_sectors, sp->segment_size_in_blocks, sp->flash_or_log_size_in_segments,
		   sp->wear_limit_for_earased_blocks, sp->next_segment_number_in_log);

	printf("Ifile info: \n\t i_num: %d\n\t file_type: %d\n\t address<segment no, block no>: <%d,%d> \n",
		   sp->i_node_of_i_file.i_num, sp->i_node_of_i_file.file_type, sp->i_node_of_i_file.indirect_block.segment_no, sp->i_node_of_i_file.indirect_block.block_no);

    printf("Size of super segment: %ld byte\n", sizeof(supersegment));
	Log_Write_IFile_First_Time();
	Flash_Close(flash);

	return 0;
}

// a function to initialize the supersegment global variable from the flash by opening and reading the first sector from it.
int initialize_log_system(char *new_file_name, int cache_size)
{
	printf("Initializing the log file...\n");
	int *blocks = (int *)calloc(1, sizeof(int));

	Flash *flash;
	flash = Flash_Open(new_file_name, FLASH_SILENT, blocks);
	if (flash == NULL)
	{
		printf("Error opening the flash file.\n");
		return -1;
	}
	printf("Flash opened successfully.\n");

	int size_segment_in_sectors = 64;

	// sp is the global variable for the supersegment
	sp = (supersegment *)calloc(size_segment_in_sectors * 512, sizeof(char));
	int r = Flash_Read(flash, 0, size_segment_in_sectors, sp);
	printf("Result of reading the Super Segment: %d - Error no: %d\n", r, errno);

	Flash_Close(flash);

    flash_drive_name = new_file_name;

    temporary_buffer = (void *)calloc(size_segment_in_sectors * 512, sizeof(char));
    tail_write_buffer = (void *)calloc(size_segment_in_sectors * 512, sizeof(char));

	int max_num_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
	IFile = (i_node_block_address_mapping **)calloc(max_num_mappings, sizeof(i_node_block_address_mapping *));
	Log_Read_IFile();

	for (int i = 0; i < maxEntitiesPerSegment; ++i)
	{
		strcpy(recent_segment_entities[i].description, "Ununsed Block");
		if (i == 0)
		{
			strcpy(recent_segment_entities[i].description, "Segment Summary Block");
		}
	}

    num_segments_cached = cache_size;
    cache_segments_ptr = (void **)calloc(num_segments_cached, sizeof(void *));
	int size_segment_in_bytes = size_segment_in_sectors * 512;

	for (int i = 0; i < num_segments_cached; ++i)
	{
        cache_segments_ptr[i] = (void *)calloc(size_segment_in_bytes, sizeof(char));
	}

    free_segments_idx = (int *)calloc(num_segments_cached, sizeof(int));
	for (int i = 0; i < num_segments_cached; ++i)
	{
        free_segments_idx[i] = -1;
	}

	return 0;
}

//This is a function that reads a specific sector to a buffer.
//The flash read function reads in sectors of 512 bytes each. Each segment is made up of 32 blocks,
//and each block is made up of 2 sectors. Therefore, each segment is 64 sectors.
//We need to read 64 sectors at a time, or an array of size 64 * 512 bytes
// (or size of char). The buffer has a size of 512 * block size in sectors (2). Log_read is complete.
int Log_Read(block_address address, int length, void *buffer)
{
	int segment_no = address.segment_no;
	int block_no = address.block_no;

	int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors;
    // Check if the segment is the current write segment
	if (segment_no == sp->next_segment_number_in_log)
	{
		printf(">>>> reading from WRITE (mem) segment. address: <%d,%d> length: %d\n", segment_no, block_no, length);
        // Copy the data from the write buffer
        memcpy(buffer, tail_write_buffer + block_no * sp->block_size_in_sectors * 512, length);
		printf("Successfully read.\n");
	}
    // Check if the segment is already cached
    else if ((index_segment_available_in_cache(segment_no) != -1))
	{
        printf(">>>> reading from CACHED (mem) segment at %d.\n", index_segment_available_in_cache(segment_no));
        // Copy the data from the cache
        memcpy(buffer, cache_segments_ptr[index_segment_available_in_cache(segment_no)] + block_no * sp->block_size_in_sectors * 512, length);
    }
    // Otherwise, read the data from the flash drive
    else
    {
        printf(">>>> reading from FLASH segment.\n");
        // Open the flash drive
        int *blocks = (int *)calloc(1, sizeof(int));
        Flash *flash;
        flash = Flash_Open(flash_drive_name, FLASH_SILENT, blocks);
        Flash_Read(flash, segment_no * size_segment_in_sectors, size_segment_in_sectors, temporary_buffer);
        // Copy the data from the temporary buffer to the buffer
        int i = 0;
        while (i < length)
        {
            ((char *)buffer)[i] = ((char *)temporary_buffer)[block_no * sp->block_size_in_sectors * 512 + i];
            i++;
        }
        // Close the flash drive
        Flash_Close(flash);
        // If there is space in the cache, cache the segment
        if (num_segments_cached > 0)
        {
            int new_cache_index = get_new_cache_index();
            printf(">>>> ** Caching at %d\n", new_cache_index);
            memcpy(cache_segments_ptr[new_cache_index], temporary_buffer, size_segment_in_sectors * 512);
            free_segments_idx[new_cache_index] = segment_no;
        }
    }
	return 0;
}

int get_new_cache_index()
{
	for (int i = 0; i < num_segments_cached; ++i)
	{
		if (free_segments_idx[i] == -1)
		{
			return i;
		}
	}

	return rand() % (num_segments_cached);
}

int index_segment_available_in_cache(int segment_no)
{
	for (int i = 0; i < num_segments_cached; ++i)
	{
		if (free_segments_idx[i] == segment_no)
		{
			return i;
		}
	}
	return -1;
}

// This is a function that writes a buffer to a specific sector or sectors of a flash drive.
// manages the inode for the file that is being written.
// The only thing left to do in log.c is to handle the blocks, ifile, and inodes.
int Log_Write(block_address *saved_address, int file_inum, int file_block_no, int length, void *buffer, char *desc)
{
    // Copy the data from the buffer to the write buffer
	memcpy(tail_write_buffer + (last_written_block_number * sp->block_size_in_sectors * 512), buffer, 512 * sp->block_size_in_sectors);
    // Set the segment and block number of the saved address
	saved_address->segment_no = sp->next_segment_number_in_log;
	saved_address->block_no = last_written_block_number;
    // Update the last segment summary block
	Update_Last_Segment_Summary_Block(last_written_block_number, desc);
    // Increment the last written block number
    last_written_block_number++;
    // If the last written block number is equal to the size of the segment minus 15, flush the segment to disk
	if (last_written_block_number == sp->segment_size_in_blocks - 15)
	{
		flush_tail_segment_to_disk();
	}
	return 0;
}

int flush_tail_segment_to_disk()
{
    last_written_block_number = 1;
	int *blocks = (int *)calloc(1, sizeof(int));
	Flash *flash;
	flash = Flash_Open(flash_drive_name, FLASH_SILENT, blocks);
	int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors; // 32*2=64
	// printf("last segment number in log: %d\n", sp->next_segment_number_in_log);
	int result = Flash_Write(flash, sp->next_segment_number_in_log * size_segment_in_sectors, size_segment_in_sectors, (void *)tail_write_buffer);
	Flash_Close(flash);
	(sp->next_segment_number_in_log)++;
	free(tail_write_buffer);
    tail_write_buffer = (void *)calloc(size_segment_in_sectors * 512, sizeof(char));
	Update_SuperSegment();
}

// it is used to load the IFILE from the segment 1 to the memory for easy use
int Log_Read_IFile()
{
	// 4 is the number of direct blocks
	void *buffer = (void *)calloc((4 * sp->block_size_in_sectors * 512), sizeof(char));
	Log_Read(sp->i_node_of_i_file.direct_block[0], (4 * sp->block_size_in_sectors * 512), buffer);
	int max_num_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
    int i = 0;
    while (i < max_num_mappings)
    {
        IFile[i] = (i_node_block_address_mapping *)calloc(1, sizeof(i_node_block_address_mapping));
        memcpy(IFile[i], buffer + (i * sizeof(i_node_block_address_mapping)), sizeof(i_node_block_address_mapping));
        i++;
    }
	// inodeFilePrint();
}

void inodeFilePrint()
{
//    int max_num_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
    for (int i = 0; i < sp->i_node_mapping_count; ++i)
    {
        printf("inum: %d, address: <%d,%d>.\n", IFile[i]->i_num, IFile[i]->i_node_address.segment_no, IFile[i]->i_node_address.block_no);
    }
}

int Log_Write_IFile_First_Time()
{
    // allocate a buffer for the mappings
    void *buffer = (void *)calloc((4 * sp->block_size_in_sectors * 512), sizeof(char));
	int max_num_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
    // iterate over the mappings and initialize them
    int i = 0;
    while (i < max_num_mappings)
    {
        i_node_block_address_mapping *tmp = (i_node_block_address_mapping *)calloc(1, sizeof(i_node_block_address_mapping));
        tmp->i_num = -1;
        tmp->i_node_address.segment_no = -1;
        tmp->i_node_address.block_no = -1;
        memcpy(buffer + (i * sizeof(i_node_block_address_mapping)), tmp, sizeof(i_node_block_address_mapping));
        i++;
    }
    // allocate a pointer for the blocks
	int *blocks = (int *)calloc(1, sizeof(int));
    // open the flash
	Flash *flash;
	flash = Flash_Open(flash_drive_name, FLASH_SILENT, blocks);
	int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors;
    // get the address of the sector in the file
	int sector_address = (sp->i_node_of_i_file.direct_block[0].segment_no * sp->segment_size_in_blocks + sp->i_node_of_i_file.direct_block[0].block_no) * sp->block_size_in_sectors;
	int address_in_erase_blocks = sector_address / 16;
	Log_Erase(1, 1);
    // Write the buffer to the flash
	int result = Flash_Write(flash, sector_address, (4 * sp->block_size_in_sectors), (void *)buffer);
	Flash_Close(flash);
	return 0;
}

// used to write the updates of the IFILE to the segment 1 - first erase and then write
int Log_Write_IFile()
{
    // allocate a buffer for the mappings
    void *buffer = (void *)calloc((4 * sp->block_size_in_sectors * 512), sizeof(char));
	int max_num_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
    // iterate over the mappings and copy them to the buffer
    int i = 0;
    while (i < max_num_mappings)
    {
        memcpy(buffer + (i * sizeof(i_node_block_address_mapping)), IFile[i], sizeof(i_node_block_address_mapping));
        i++;
    }
    // allocate a pointer for the blocks
	int *blocks = (int *)calloc(1, sizeof(int));
    // open the flash
    Flash *flash;
	flash = Flash_Open(flash_drive_name, FLASH_SILENT, blocks);
	int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors; // 32*2=64
	int sector_address = (sp->i_node_of_i_file.direct_block[0].segment_no * sp->segment_size_in_blocks + sp->i_node_of_i_file.direct_block[0].block_no) * sp->block_size_in_sectors;
	int address_in_erase_blocks = sector_address / 16;
	Log_Erase(1, 1);
	int result = Flash_Write(flash, sector_address, (4 * sp->block_size_in_sectors), (void *)buffer);
	Flash_Close(flash);
	return 0;
}

// updates the IFILE on creation of a new file
int Log_Add_mapping(i_node_block_address_mapping mapping)
{
    // copy the mapping to the file
    memcpy(IFile[sp->i_node_mapping_count], &mapping, sizeof(i_node_block_address_mapping));
    // Increment the mapping count
    (sp->i_node_mapping_count)++;
    // write the file to the flash
    Log_Write_IFile();
    // update the super segment
    Update_SuperSegment();
	return 0;
}

int Log_Remove_mapping(int inum)
{
	int max_num_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
	int indx = -1;
    int i = 0;
    while (i < sp->i_node_mapping_count) {
        if (IFile[i]->i_num == inum) {
            indx = i;
            break;
        }
        i++;
    }
    // if the mapping was not found, print an error message and return -1.
    if (indx == -1) {
        printf("Error: The specified inum could not be found.\n");
        return -1;
    }
    // shift the mappings after the removed mapping one position to the left.
    while (indx < sp->i_node_mapping_count - 1) {
        IFile[indx]->i_num = IFile[indx + 1]->i_num;
        IFile[indx]->i_node_address.segment_no = IFile[indx + 1]->i_node_address.segment_no;
        IFile[indx]->i_node_address.block_no = IFile[indx + 1]->i_node_address.block_no;
        indx++;
    }
    // set the last mapping to -1
    IFile[sp->i_node_mapping_count]->i_num = -1;
	IFile[sp->i_node_mapping_count]->i_node_address.segment_no = -1;
	IFile[sp->i_node_mapping_count]->i_node_address.block_no = -1;
    // decrement the mapping count
    (sp->i_node_mapping_count)--;
	Log_Write_IFile();
    // update the super segment
    Update_SuperSegment();
	return 0;
}

// erase a specific part of the flash (units are in erase blocks)
int Log_Erase(int segment_no, int length_in_segments)
{
    // Allocate a pointer for the blocks.
    int *blocks = (int *)calloc(1, sizeof(int));
    // Open the flash.
	Flash *flash;
	flash = Flash_Open(flash_drive_name, FLASH_SILENT, blocks);
	Flash_Erase(flash, segment_no * (sp->segment_size_in_blocks * sp->block_size_in_sectors) / 16, length_in_segments * (sp->segment_size_in_blocks * sp->block_size_in_sectors) / 16);
    // Close the flash.
    Flash_Close(flash);
	return 0;
}

// updates the supersegment at segment zero
int Update_SuperSegment()
{
	int *blocks = (int *)calloc(1, sizeof(int));
    // Open the flash.
    Flash *flash;
	flash = Flash_Open(flash_drive_name, FLASH_SILENT, blocks);
    // Get the size of a segment in sectors.
    int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors;
    // Erase the first segment.
    Log_Erase(0, 1);
	int w = Flash_Write(flash, 0, size_segment_in_sectors, (void *)sp);
	Flash_Close(flash);
    // Return the result of the write operation.
    return w;
}

int Update_Last_Segment_Summary_Block(int block_no, char *desc)
{
	if (block_no > 31)
	{
		printf("The block number exceeds the maximum limit of the segment summary block size.\n");
		return -1;
	}
    // check if the description length is within the limit.
    if (strlen(desc) > 31)
	{
		printf("The description exceeds the maximum allowed length.\n");
		return -1;
	}
    // copy the description to the segment summary block
    strcpy(recent_segment_entities[block_no].description, desc);
    // copy the segment summary block to the tail write buffer
    memcpy(tail_write_buffer, recent_segment_entities, 512 * sp->block_size_in_sectors);
	return 0;
}

segment_summary_entity **Read_Segment_Summary_Block(int segment_no)
{
    // allocate a buffer for the segment summary block
    void *buffer = (void *)calloc((sp->block_size_in_sectors * 512), sizeof(char));
	block_address addr;
	addr.segment_no = segment_no;
	addr.block_no = 0;
    // read the segment summary block from the log
	Log_Read(addr, (sp->block_size_in_sectors * 512), buffer);
    // allocate an array of pointers to segment summary entities
    segment_summary_entity **entities = (segment_summary_entity **)calloc(maxEntitiesPerSegment, sizeof(segment_summary_entity *));
    // iterate over the segment summary block and copy the entities to the array
    int i = 0;
    while (i < maxEntitiesPerSegment)
    {
        entities[i] = (segment_summary_entity *)calloc(1, sizeof(segment_summary_entity));
        memcpy(entities[i], buffer + (i * sizeof(segment_summary_entity)), sizeof(segment_summary_entity));
        i++;
    }

	return entities;
}