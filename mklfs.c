#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "flash.h"
#include "log.h"
#include "directory.h"

int main(int argc, char *argv[])
{
	int block_size_in_sectors = 2;
	int segment_size_in_blocks = 32;
	int flash_or_log_size_in_segments = 100;
	int wear_limit_for_earased_blocks = 1000;
	char *file_name = "sadaf";

	// file_name = argv[argc-1];
	// printf("file_name: %s\n", file_name);

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-b") == 0)
		{
			block_size_in_sectors = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-l") == 0)
		{
			segment_size_in_blocks = atoi(argv[i + 1]);
			if (segment_size_in_blocks % 16 != 0)
			{
				printf("Segment size in blocks should be a multiple of erasure block in flash (ie. 16).\n");
				return 1;
			}
		}
		else if (strcmp(argv[i], "-s") == 0)
		{
			flash_or_log_size_in_segments = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-w") == 0)
		{
			wear_limit_for_earased_blocks = atoi(argv[i + 1]);
		}
	}

	Log_Create(file_name, block_size_in_sectors, segment_size_in_blocks, flash_or_log_size_in_segments, wear_limit_for_earased_blocks);
	Log_Initialize(file_name, 0);
	directory_create(-1, "/");
	Flush_Tail_Segment_To_Disk();
	return 0;
}
