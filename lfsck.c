#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include "log.h"
#include "file.h"
#include "directory.h"
#include "lfsck.h"

int main(int argc, char *argv[])
{

	// write a call int Log_Initialize(char *file_n,int cache_size) to initialize the log
	Log_Initialize(argv[argc - 1], 0);

	Print_IFile();
	print_segment_summary_blocks();
	in_use_inodes_with_no_data();
	useless_entries();

	return 0;
}

void print_segment_summary_blocks()
{
	printf("********** Showing All the Segment Summary Blocks ********** \n\n");

	int total_segment = sp->next_segment_number_in_log;
	printf("Total number of written segments: %d\n\n", total_segment);
	for (int i = 3; i < total_segment; ++i)
	{
		segment_summary_entity **ent = Read_Segment_Summary_Block(i);
		printf("------------- Segment Summary For Segment #%3d -------------\n\n", i);

		for (int j = 0; j < 32; ++j)
		{
			printf(">>> %2d: %s\n", j, ent[j]->description);
		}
		printf("------------------------------------------------------------\n");
	}

	printf("************************************************************\n\n");
}

void in_use_inodes_with_no_data()
{
	printf("********** Showing In Use Inodes In IFile With No Directory Entry ********** \n\n");

	int total_segment = sp->next_segment_number_in_log;
	for (int i = 0; i < sp->i_node_mapping_count; ++i)
	{
		// printf("inum: %d, address: <%d,%d>.\n", IFile[i]->i_num, IFile[i]->i_node_address.segment_no,IFile[i]->i_node_address.block_no);
		if (IFile[i]->i_node_address.segment_no >= total_segment)
		{
			printf(">>> (In Use Inode With No Directory Entry - SEGMENT not exitsts):\n    inum > %-3d @ [seg. %-3d,blck. %-3d]\n", IFile[i]->i_num,
				   IFile[i]->i_node_address.segment_no, IFile[i]->i_node_address.block_no);

			char *type = "FILE";
			if (get_file_type(IFile[i]->i_num) == 1)
			{
				type = "DIRECTORY";
			}

			printf("    This inum belongs to a %s.\n\n", type);
		}
		else
		{
			segment_summary_entity **ent = Read_Segment_Summary_Block(IFile[i]->i_node_address.segment_no);
			if (strstr(ent[IFile[i]->i_node_address.block_no]->description, ":") == NULL)
			{
				printf(">>> (In Use Inode With No Directory Entry - BLOCK   not exitsts):\n    inum > %-3d @ [seg. %-3d,blck. %-3d]\n", IFile[i]->i_num,
					   IFile[i]->i_node_address.segment_no, IFile[i]->i_node_address.block_no);

				char *type = "FILE";
				if (get_file_type(IFile[i]->i_num) == 1)
				{
					type = "DIRECTORY";
				}

				printf("    This inum belongs to a %s.\n\n", type);
			}
		}
	}

	printf("****************************************************************************\n\n");
}

void useless_entries()
{
	printf("********** Showing Removed Or Old Entries or Blocks ********** \n\n");

	int total_segment = sp->next_segment_number_in_log;

	for (int i = 3; i < total_segment; ++i)
	{
		segment_summary_entity **ent = Read_Segment_Summary_Block(i);

		for (int j = 0; j < 32; ++j)
		{
			char *tmp = (char *)calloc(strlen(ent[j]->description), sizeof(char));
			strncpy(tmp, ent[j]->description, strstr(ent[j]->description, " ") - ent[j]->description);

			if (strcmp(tmp, "Inode") == 0)
			{
				int inum_from_segment_summary = atoi(strstr(ent[j]->description, ":") + 2);

				if (inode_address_conflict_with_ifile(inum_from_segment_summary, i, j))
				{
					printf(">>> (Deleted or Useless Entry - OLD INODE):\n    inum > %-3d @ [seg. %-3d,blck. %-3d]\n\n", inum_from_segment_summary, i, j);
				}
			}
			else if (strcmp(tmp, "Indirect") == 0)
			{
				int inum_from_segment_summary = atoi(strstr(ent[j]->description, ":") + 2);

				if (indirect_address_conflict_with_inode(inum_from_segment_summary, i, j))
				{
					printf(">>> (Deleted or Useless Entry - OLD INDIRECT BLOCK):\n    inum > %-3d @ [seg. %-3d,blck. %-3d]\n\n", inum_from_segment_summary, i, j);
				}
			}
			else if (strcmp(tmp, "Data") == 0)
			{
				int inum_from_segment_summary = atoi(strstr(ent[j]->description, ":") + 2);

				if (data_address_conflict_with_inode(inum_from_segment_summary, i, j))
				{
					printf(">>> (Deleted or Useless Entry - OLD DATA BLOCK):\n    inum > %-3d @ [seg. %-3d,blck. %-3d]\n\n", inum_from_segment_summary, i, j);
				}
			}
		}
	}

	printf("************************************************************** \n\n");
}

int indirect_address_conflict_with_inode(int inum_from_segment_summary, int segment_no, int block_no)
{
	i_node *inode = get_inode(inum_from_segment_summary);

	if (inode->indirect_block.segment_no == segment_no && inode->indirect_block.block_no == block_no)
	{
		return 0;
	}

	return 1;
}

int data_address_conflict_with_inode(int inum_from_segment_summary, int segment_no, int block_no)
{
	i_node *inode = get_inode(inum_from_segment_summary);

	for (int i = 0; i < 3; ++i)
	{
		if (inode->direct_block[i].segment_no == segment_no && inode->direct_block[i].block_no == block_no)
		{
			return 0;
		}
	}

	block_address indirect_addresses[sp->block_size_in_sectors * 512 / sizeof(block_address)];
	Log_Read(inode->indirect_block, sp->block_size_in_sectors * 512, (void *)(&indirect_addresses));

	for (int i = 0; i < sp->block_size_in_sectors * 512 / sizeof(block_address); ++i)
	{
		if (indirect_addresses[i].segment_no == segment_no && indirect_addresses[i].block_no == block_no)
		{
			return 0;
		}
	}

	return 1;
}

int inode_address_conflict_with_ifile(int inum_from_segment_summary, int segment_no, int block_no)
{
	for (int i = 0; i < sp->i_node_mapping_count; ++i)
	{
		if (IFile[i]->i_num == inum_from_segment_summary)
		{
			if (IFile[i]->i_node_address.segment_no != segment_no || IFile[i]->i_node_address.block_no != block_no)
			{
				return 1;
			}
			else
			{
				return 0;
			}
		}
	}

	return 1;
}