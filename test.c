#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "file.h"
#include "directory.h"
#include <errno.h>


int main(int argc, char *argv[])
{
	// print hello world
	Log_Initialize("zaniar",0);
	// Print_IFile();

	// char *aa = (char *)calloc(512*64,sizeof(char));
	// aa[0] = 'a';aa[1] = 'r';aa[2] = 'i';aa[3] = 'a';aa[4] = 'n';aa[5] = '\n';


	// for (int i = 0; i < 35; ++i)
	// {
	// 	aa[4] = i+97;
	// 	block_address *address = (block_address *)calloc(1,sizeof(block_address));
	// 	Log_Write(address,0,0,0, aa);
	// }

	// char *n = (char *)calloc(512*64,sizeof(char));
	

	// for (int i = 0; i < 32; ++i)
	// {
	// 	block_address address;
	// 	address.segment_no = 2;
	// 	address.block_no = i;

	// 	Log_Read(address,512,n);
	// 	printf("%s\n",(char*)n);
	// }	

	// file_create(47);
	// file_create(1);
	// file_create(1);
	// file_create(1);
	// file_create(2);
	// Log_Remove_mapping(7);

	// Print_IFile();

	// char *buffer = (char *)calloc(33*1024,sizeof(char));
	// memcpy(buffer,"salam jahan!!! :D",18);
	// char *buffer = "<THIS IS FIRST PART>";
	// file_write(1,0,strlen(buffer),(void *)buffer);

	// ***************** lfsck ******************
	// segment_summary_entity **ent = Read_Segment_Summary_Block(3);

	// for (int i = 0; i < 32; ++i)
	// {
	// 	printf("block %d: %s\n", i,ent[i]->description);
	// }

	// ******************************************

	// char *buffer = "<THIS IS SECOND PART>";
	// file_write(2,0,strlen(buffer),(void *)buffer);

	// char *buffer1 = (char *)calloc(50,sizeof(char));
	// file_read_begining_to_end(1,(void *)buffer1);
	// printf("%s\n", buffer1);


	// char *buffer1 = (char *)calloc(33*1024,sizeof(char));
	// file_read(1,0,33*1024,buffer1);
	// printf("%s\n", buffer1);

	// for (int i = 0; i < sp->i_node_mapping_count; ++i)
	// {
	// 	printf("inum: %d , address<seg no,blc no>: <%d,%d>\n",IFile[i]->i_num,IFile[i]->i_node_address.segment_no,IFile[i]->i_node_address.block_no);
	// }

	// ******************

	// i_node *tmp = get_inode(1);
	// block_address indirect_addresses[sp->block_size_in_sectors*512/sizeof(block_address)];
	// Log_Read(tmp->indirect_block,1024,(void *)(&indirect_addresses));
		
	// for (int i = 0; i < 10; ++i)
	// {
	// 	if (i<4)
	// 	{
	// 		printf("direct block %d address: <%d,%d>\n",i, tmp->direct_block[i].segment_no,tmp->direct_block[i].block_no);

	// 	}
	// 	else
	// 	{
	// 		printf("indirect block %d address: <%d,%d>\n",i-4, indirect_addresses[i-4].segment_no,indirect_addresses[i-4].block_no);
	// 	}
	// }

	// *******************

	// printf("inum: %d, first_block_address: <%d,%d>, file type: %d\n",tmp->i_num,tmp->direct_block[0].segment_no,tmp->direct_block[0].block_no,tmp->file_type);

	// block_address tmp = get_block_address(1,0);
	// printf("block_address: <%d,%d>\n",tmp.segment_no,tmp.block_no);

	// int t;
	// block_address **tmp = get_affected_blocks_addresses(3,0,1025,&t);
	// printf("%d\n", t);

	// printf("%ld\n", sizeof(block_address));
	
	// block_address indirect_addresses[sp->block_size_in_sectors*512/sizeof(block_address)];
	// printf("%ld\n", sizeof(indirect_addresses));
	
	// printf("%d\n", 7%5);

	// **************** Directory test ***********************


	directory_create(1,"a");
	directory_create(1,"b");
	directory_create(1,"c");
	printf("three directory created.\n");
	print_new_entries(1);
	// Flush_Tail_Segment_To_Disk();
	directory_remove(3);

	print_new_entries(1);
	
	// print something here



	// int *dir_count = (int *)calloc(1,sizeof(int));
	// directory_entry **entries = get_subdirectories(1,dir_count);
	// printf("subdirectory counts: %d\n", *dir_count);
	// for (int i = 0; i < *dir_count; ++i)
	// {
	// 	printf("dir name: %s, inum: %d\n", entries[i]->name,entries[i]->i_num);
	// }

	// *******************************************************

	// Flush_Tail_Segment_To_Disk();

	// i_node *tmp = get_inode(1);
	// printf("First block address: <%d,%d>\n", tmp->direct_block[0].segment_no,tmp->direct_block[0].block_no);
	
	// printf("%d\n", sizeof(segment_summary_entity));

	return 0;
}
