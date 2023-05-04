#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "flash.h"
#include "log.h"
#include <errno.h>

// the name of the file that resembels the flash drive
char *file_name = "tmp_flash";

// The temporary segment used to read the requested segment
void *last_segment_read;

// The tail segment. we write first to this segment and when filled, write it to the log
void *last_segment_write;

// keeps the changes in the last segment
const int segment_summary_entity_no = 32;
segment_summary_entity last_segment_summary_block[32];

// The last written block number in the tail segment
int last_block_number_in_segment = 1;

// caching
int total_segments_in_cache = 0;
void **cached_segments;
int *available_segments;

int getFlashSize() {
	return (sp->flash_or_log_size_in_segments * sp->segment_size_in_blocks * sp->block_size_in_sectors) / 16;
}

int getSegmentSizeInSectors() {
	return sp->segment_size_in_blocks * sp->block_size_in_sectors;
}

// the function to create a flash file and saves the metadata on that. It is called from mklfs
int log_create(char *file_n, int bl_size, int seg_size, int log_size, int wear_lim)
{
	// Saving the metadata paramenters in the supersegment global variable
	sp = (supersegment *)calloc(1, sizeof(supersegment));

	sp->block_size_in_sectors = bl_size;
	sp->segment_size_in_blocks = seg_size;
	sp->flash_or_log_size_in_segments = log_size;
	sp->wear_limit_for_earased_blocks = wear_lim;
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
	Flash_Create(file_n, sp->wear_limit_for_earased_blocks,  getFlashSize());

	// -------- Opening the flash and saving the metadata in the super segment (first segment) --------

	// a space to save the total number of blocks
	u_int *blocks = (u_int *)calloc(1, sizeof(int));

	Flash *flash;
	flash = Flash_Open(file_n, FLASH_SILENT, blocks);

	// int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors;

	int r = Flash_Write(flash, 0, getSegmentSizeInSectors(), (void *)sp);

	printf("Writing Supper Segment result: %d - Error no: %d\n", r, errno);

	printf("Successfully created the FLASH.\nblock_size_in_sectors = %d\nint segment_size_in_blocks = %d\nint flash_or_log_size_in_segments "
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
int initialize_log_system(char *file_n, int cache_size)
{
	printf("Initializing the log file...\n");
	u_int *blocks = (u_int *)calloc(1, sizeof(int));

	Flash *flash;
	flash = Flash_Open(file_n, FLASH_SILENT, blocks);
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
	printf("Reading Supper Segment result: %d - Error no: %d\n", r, errno);

	Flash_Close(flash);

	file_name = file_n;

	last_segment_read = (void *)calloc(size_segment_in_sectors * 512, sizeof(char));
	last_segment_write = (void *)calloc(size_segment_in_sectors * 512, sizeof(char));

	int max_number_of_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
	IFile = (i_node_block_address_mapping **)calloc(max_number_of_mappings, sizeof(i_node_block_address_mapping *));
	Log_Read_IFile();

	for (int i = 0; i < segment_summary_entity_no; ++i)
	{
		strcpy(last_segment_summary_block[i].description, "Ununsed Block");
		if (i == 0)
		{
			strcpy(last_segment_summary_block[i].description, "Segment Summary Block");
		}
	}

	total_segments_in_cache = cache_size;
	cached_segments = (void **)calloc(total_segments_in_cache, sizeof(void *));
	int size_segment_in_bytes = size_segment_in_sectors * 512;

	for (int i = 0; i < total_segments_in_cache; ++i)
	{
		cached_segments[i] = (void *)calloc(size_segment_in_bytes, sizeof(char));
	}

	available_segments = (int *)calloc(total_segments_in_cache, sizeof(int));
	for (int i = 0; i < total_segments_in_cache; ++i)
	{
		available_segments[i] = -1;
	}

	return 0;
}

// a function to read a specific sector to a buffer
// flash read, reads in sectors (512 byte or sizeof(char))
// each segment is <sp->segment_size_in_blocks> blocks (32) and each block is <sp->block_size_in_sectors> sectors (2).
// thus each segment is 64 sectors. we need to read 64 sectors at a time. or an array of size 64 * 512 bytes (or size of char)
// buffer has size 512 * block size in sector (2).
// Log_read is complete
int Log_Read(block_address address, int length, void *buffer)
{
	int segment_no = address.segment_no;
	int block_no = address.block_no;

	int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors;

	if (segment_no == sp->next_segment_number_in_log)
	{
		printf(">>>> reading from WRITE (mem) segment. address: <%d,%d> length: %d\n", segment_no, block_no, length);

		// for (int i = 0; i < length; ++i)
		// {
		// 	((char *)buffer)[i] = ((char *)last_segment_write)[block_no*sp->block_size_in_sectors*512+i];
		// }

		memcpy(buffer, last_segment_write + block_no * sp->block_size_in_sectors * 512, length);
		printf("Successfully read.\n");
	}
	else
	{
		int index_cache = index_segment_available_in_cache(segment_no);
		if (index_cache != -1)
		{
			printf(">>>> reading from CACHED (mem) segment at %d.\n", index_cache);

			memcpy(buffer, cached_segments[index_cache] + block_no * sp->block_size_in_sectors * 512, length);
		}
		else
		{
			printf(">>>> reading from FLASH segment.\n");

			u_int *blocks = (u_int *)calloc(1, sizeof(int));

			Flash *flash;

			flash = Flash_Open(file_name, FLASH_SILENT, blocks);

			Flash_Read(flash, segment_no * size_segment_in_sectors, size_segment_in_sectors, last_segment_read);

			for (int i = 0; i < length; ++i)
			{
				((char *)buffer)[i] = ((char *)last_segment_read)[block_no * sp->block_size_in_sectors * 512 + i];
			}

			Flash_Close(flash);

			if (total_segments_in_cache > 0)
			{
				int new_cache_index = get_new_cache_index();
				printf(">>>> ** Caching at %d\n", new_cache_index);
				memcpy(cached_segments[new_cache_index], last_segment_read, size_segment_in_sectors * 512);
				available_segments[new_cache_index] = segment_no;
			}
		}
	}

	return 0;
}

int get_new_cache_index()
{
	for (int i = 0; i < total_segments_in_cache; ++i)
	{
		if (available_segments[i] == -1)
		{
			return i;
		}
	}

	return rand() % (total_segments_in_cache);
}

int index_segment_available_in_cache(int segment_no)
{
	for (int i = 0; i < total_segments_in_cache; ++i)
	{
		if (available_segments[i] == segment_no)
		{
			return i;
		}
	}

	return -1;
}

// a function to write a buffer to a specific sector(s) of a flash drive.
// to do: manage inode for the file that we are writing one of its blocks.
// The only thing that is left in log.c is handling the blocks and ifile and inodes.
int Log_Write(block_address *saved_address, int file_inum, int file_block_no, int length, void *buffer, char *desc)
{

	memcpy(last_segment_write + (last_block_number_in_segment * sp->block_size_in_sectors * 512), buffer, 512 * sp->block_size_in_sectors);

	saved_address->segment_no = sp->next_segment_number_in_log;
	saved_address->block_no = last_block_number_in_segment;

	Update_Last_Segment_Summary_Block(last_block_number_in_segment, desc);

	last_block_number_in_segment++;

	// printf("This block was written at block index %d on the tail segment in memory.\n", last_block_number_in_segment-1);

	if (last_block_number_in_segment == sp->segment_size_in_blocks - 15)
	{
		flush_tail_segment_to_disk();
	}

	return 0;
}

int flush_tail_segment_to_disk()
{
	last_block_number_in_segment = 1;

	u_int *blocks = (u_int *)calloc(1, sizeof(int));

	Flash *flash;

	flash = Flash_Open(file_name, FLASH_SILENT, blocks);

	int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors; // 32*2=64
	// printf("last segment number in log: %d\n", sp->next_segment_number_in_log);
	Flash_Write(flash, sp->next_segment_number_in_log * size_segment_in_sectors, size_segment_in_sectors, (void *)last_segment_write);

	// printf("Write Result: %d\n", result);
	// printf("\t Error no: %d\n", errno);

	Flash_Close(flash);

	(sp->next_segment_number_in_log)++;

	free(last_segment_write);
	last_segment_write = (void *)calloc(size_segment_in_sectors * 512, sizeof(char));

	// printf("The tail segment was written to the log at segment index %d.\n", sp->next_segment_number_in_log-1);

	Update_SuperSegment();

	return 0;
}

// it is used to load the IFILE from the segment 1 to the memory for easy use
int Log_Read_IFile()
{
	// 4 is the number of direct blocks

	void *buffer = (void *)calloc((4 * sp->block_size_in_sectors * 512), sizeof(char));

	Log_Read(sp->i_node_of_i_file.direct_block[0], (4 * sp->block_size_in_sectors * 512), buffer);

	int max_number_of_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);

	for (int i = 0; i < max_number_of_mappings; ++i)
	{
		IFile[i] = (i_node_block_address_mapping *)calloc(1, sizeof(i_node_block_address_mapping));

		memcpy(IFile[i], buffer + (i * sizeof(i_node_block_address_mapping)), sizeof(i_node_block_address_mapping));
	}

	// Print_IFile();
	return 0;
}

void Print_IFile()
{
	// int max_number_of_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
	printf("********** IFILE **********\n\n");

	for (int i = 0; i < sp->i_node_mapping_count; ++i)
	{
		printf("inum: %d, address: <%d,%d>.\n", IFile[i]->i_num, IFile[i]->i_node_address.segment_no, IFile[i]->i_node_address.block_no);
	}
	printf("___________________________\n\n");
}

int Log_Write_IFile_First_Time()
{
	void *buffer = (void *)calloc((4 * sp->block_size_in_sectors * 512), sizeof(char));

	int max_number_of_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);

	for (int i = 0; i < max_number_of_mappings; ++i)
	{
		i_node_block_address_mapping *tmp = (i_node_block_address_mapping *)calloc(1, sizeof(i_node_block_address_mapping));
		tmp->i_num = -1;
		tmp->i_node_address.segment_no = -1;
		tmp->i_node_address.block_no = -1;
		memcpy(buffer + (i * sizeof(i_node_block_address_mapping)), tmp, sizeof(i_node_block_address_mapping));
	}

	u_int *blocks = (u_int *)calloc(1, sizeof(int));

	Flash *flash;

	flash = Flash_Open(file_name, FLASH_SILENT, blocks);

	// int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors; // 32*2=64

	int address_in_sector = (sp->i_node_of_i_file.direct_block[0].segment_no * sp->segment_size_in_blocks + sp->i_node_of_i_file.direct_block[0].block_no) * sp->block_size_in_sectors;

	// int address_in_erase_blocks = address_in_sector / 16;

	Log_Erase(1, 1);

	Flash_Write(flash, address_in_sector, (4 * sp->block_size_in_sectors), (void *)buffer);

	// printf("IFILE UPDATE-Write Result: %d\n", result);
	// printf("\t Error no: %d\n", errno);

	Flash_Close(flash);

	return 0;
}

// used to write the updates of the IFILE to the segment 1 - first erase and then write
int Log_Write_IFile()
{
	void *buffer = (void *)calloc((4 * sp->block_size_in_sectors * 512), sizeof(char));

	int max_number_of_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);

	for (int i = 0; i < max_number_of_mappings; ++i)
	{
		memcpy(buffer + (i * sizeof(i_node_block_address_mapping)), IFile[i], sizeof(i_node_block_address_mapping));
	}

	u_int *blocks = (u_int *)calloc(1, sizeof(int));

	Flash *flash;

	flash = Flash_Open(file_name, FLASH_SILENT, blocks);

	// int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors; // 32*2=64

	int address_in_sector = (sp->i_node_of_i_file.direct_block[0].segment_no * sp->segment_size_in_blocks + sp->i_node_of_i_file.direct_block[0].block_no) * sp->block_size_in_sectors;

	// int address_in_erase_blocks = address_in_sector / 16;

	Log_Erase(1, 1);

	Flash_Write(flash, address_in_sector, (4 * sp->block_size_in_sectors), (void *)buffer);

	// printf("IFILE UPDATE-Write Result: %d\n", result);
	// printf("\t Error no: %d\n", errno);

	Flash_Close(flash);

	return 0;
}

// updates the IFILE on creation of a new file
int Log_Add_mapping(i_node_block_address_mapping mapping)
{
	memcpy(IFile[sp->i_node_mapping_count], &mapping, sizeof(i_node_block_address_mapping));

	(sp->i_node_mapping_count)++;

	Log_Write_IFile();

	Update_SuperSegment();

	return 0;
}

int Log_Remove_mapping(int inum)
{
	// int max_number_of_mappings = (4 * sp->block_size_in_sectors * 512) / sizeof(i_node_block_address_mapping);
	int index = -1;

	for (int i = 0; i < sp->i_node_mapping_count; ++i)
	{
		if (IFile[i]->i_num == inum)
		{
			index = i;
		}
	}

	if (index == -1)
	{
		printf("Delete file: inum not found.\n");
		return -1;
	}

	for (int i = index; i < sp->i_node_mapping_count; ++i)
	{
		IFile[i]->i_num = IFile[i + 1]->i_num;
		IFile[i]->i_node_address.segment_no = IFile[i + 1]->i_node_address.segment_no;
		IFile[i]->i_node_address.block_no = IFile[i + 1]->i_node_address.block_no;
	}

	IFile[sp->i_node_mapping_count]->i_num = -1;
	IFile[sp->i_node_mapping_count]->i_node_address.segment_no = -1;
	IFile[sp->i_node_mapping_count]->i_node_address.block_no = -1;
	(sp->i_node_mapping_count)--;

	Log_Write_IFile();
	Update_SuperSegment();

	return 0;
}

// erase a specific part of the flash (units are in erase blocks)
int Log_Erase(int segment_no, int length_in_segments)
{
	u_int *blocks = (u_int *)calloc(1, sizeof(int));

	Flash *flash;

	flash = Flash_Open(file_name, FLASH_SILENT, blocks);

	Flash_Erase(flash, segment_no * (sp->segment_size_in_blocks * sp->block_size_in_sectors) / 16, length_in_segments * (sp->segment_size_in_blocks * sp->block_size_in_sectors) / 16);

	Flash_Close(flash);

	return 0;
}

// updates the supersegment at segment zero
int Update_SuperSegment()
{
	u_int *blocks = (u_int *)calloc(1, sizeof(int));

	Flash *flash;
	flash = Flash_Open(file_name, FLASH_SILENT, blocks);

	int size_segment_in_sectors = sp->segment_size_in_blocks * sp->block_size_in_sectors;

	Log_Erase(0, 1);

	Flash_Write(flash, 0, size_segment_in_sectors, (void *)sp);

	Flash_Close(flash);

	return 0;
}

int Update_Last_Segment_Summary_Block(int block_no, char *desc)
{
	if (block_no > 31)
	{
		printf("Block no exceeds the limit of segment summary block size.\n");
		return -1;
	}

	if (strlen(desc) > 31)
	{
		printf("Description length exceeds the limit. \n");
		return -1;
	}

	strcpy(last_segment_summary_block[block_no].description, desc);

	memcpy(last_segment_write, last_segment_summary_block, 512 * sp->block_size_in_sectors);

	return 0;
}

segment_summary_entity **Read_Segment_Summary_Block(int segment_no)
{
	void *buffer = (void *)calloc((sp->block_size_in_sectors * 512), sizeof(char));

	block_address addr;
	addr.segment_no = segment_no;
	addr.block_no = 0;

	Log_Read(addr, (sp->block_size_in_sectors * 512), buffer);
	// printf("%s\n", (char *)buffer);
	segment_summary_entity **entities = (segment_summary_entity **)calloc(segment_summary_entity_no, sizeof(segment_summary_entity *));

	for (int i = 0; i < segment_summary_entity_no; ++i)
	{
		entities[i] = (segment_summary_entity *)calloc(1, sizeof(segment_summary_entity));

		memcpy(entities[i], buffer + (i * sizeof(segment_summary_entity)), sizeof(segment_summary_entity));
	}

	return entities;
}