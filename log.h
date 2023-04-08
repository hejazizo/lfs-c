#include <time.h>

typedef struct block_address
{
	int segment_no;
	int block_no;
} block_address;

typedef struct i_node
{
	int i_num;

	// 0 = ifile, 1 = directory, 2=normal
	int file_type;
	int file_size_in_blocks;
	int eof_index_in_bytes;

	block_address direct_block[4];
	block_address indirect_block;

	time_t modify_Time;
	time_t access_Time;
	time_t create_Time;
	time_t change_Time;

} i_node;

typedef struct i_node_block_address_mapping
{
	int i_num;
	block_address i_node_address;

} i_node_block_address_mapping;

// the struct that keeps the supersegment metadata
typedef struct supersegment
{
	int block_size_in_sectors;
	int segment_size_in_blocks;
	int flash_or_log_size_in_segments;
	int wear_limit_for_earased_blocks;
	int next_segment_number_in_log;
	int next_inum;
	i_node i_node_of_i_file;
	int i_node_mapping_count;

} supersegment;

i_node_block_address_mapping **IFile;

// the variable to keep the supersegment metadata
supersegment *sp;

typedef struct segment_summary_entity
{
	char description[31];

} segment_summary_entity;

int log_create(char *file_n, int bl_size, int seg_size, int log_size, int wear_lim);
int initialize_log_system(char *file_n, int cache_size);
int Log_Write(block_address *saved_address, int file_inum, int file_block_no, int length, void *buffer, char *desc);
int Log_Read(block_address address, int length, void *buffer);
int Log_Read_IFile();
int Log_Write_IFile();
int Log_Add_mapping(i_node_block_address_mapping mapping);
int Update_SuperSegment();
int Log_Erase(int segment_no, int length_in_segments);
void Print_IFile();
int flush_tail_segment_to_disk();
int Log_Write_IFile_First_Time();
int Log_Remove_mapping(int inum);
int Update_Last_Segment_Summary_Block(int block_no, char *desc);
segment_summary_entity **Read_Segment_Summary_Block(int segment_no);
int index_segment_available_in_cache(int segment_no);
int get_new_cache_index();
