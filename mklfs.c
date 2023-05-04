#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "flash.h"
#include "log.h"
#include "directory.h"

const int DEFAULT_BLOCK_SIZE_IN_SECTORS = 2;
const int DEFAULT_SEGMENT_SIZE_IN_BLOCKS = 32;
const int DEFAULT_FLASH_OR_LOG_SIZE_IN_SEGMENTS = 100;
const int DEFAULT_WEAR_LIMIT_FOR_ERASED_BLOCKS = 1000;
const char* DEFAULT_FILE_NAME = "flash.dat";

int main(int argc, char *argv[])
{
    int block_size_in_sectors = DEFAULT_BLOCK_SIZE_IN_SECTORS;
    int segment_size_in_blocks = DEFAULT_SEGMENT_SIZE_IN_BLOCKS;
    int flash_or_log_size_in_segments = DEFAULT_FLASH_OR_LOG_SIZE_IN_SEGMENTS;
    int wear_limit_for_erased_blocks = DEFAULT_WEAR_LIMIT_FOR_ERASED_BLOCKS;
    char* file_name = (char*)(DEFAULT_FILE_NAME);

	if (argc == 1) {
		printf("Usage: mklfs [-b block_size_in_sectors] [-l segment_size_in_blocks] [-s flash_or_log_size_in_segments] [-w wear_limit_for_erased_blocks] [file_name]\n");
		return 1;
	}

	if (argc % 2 == 1) {
		printf("Invalid number of arguments.\n");
		return 1;
	}

	if (argc % 2 == 0) {
		file_name = argv[argc - 1];
	}

	printf("----------------------------------------\n");
	printf("Creating file system with following parameters:\n");
	printf("Block size in sectors: %d sectors\n", block_size_in_sectors);
	printf("Segment size in blocks: %d blocks\n", segment_size_in_blocks);
	printf("Flash or log size in segments: %d segments\n", flash_or_log_size_in_segments);
	printf("Wear limit for erased blocks: %d\n", wear_limit_for_erased_blocks);
	printf("File name: %s \n", file_name);
	printf("----------------------------------------\n");

	for (int i = 1; i < argc - 1; i += 2) {
        if (strcmp(argv[i], "-b") == 0) {
            block_size_in_sectors = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-l") == 0) {
            segment_size_in_blocks = atoi(argv[i + 1]);
            if (segment_size_in_blocks % 16 != 0) {
                printf("Segment size in blocks should be a multiple of erasure block in flash (ie. 16).\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-s") == 0) {
            flash_or_log_size_in_segments = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-w") == 0) {
            wear_limit_for_erased_blocks = atoi(argv[i + 1]);
        }
        else {
            printf("Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

	// Create a new log with the specified parameters
	log_create(file_name, block_size_in_sectors, segment_size_in_blocks, flash_or_log_size_in_segments, wear_limit_for_erased_blocks);

	// Initialize the log file
	initialize_log_system(file_name, 0);

	// Create the root directory
	directory_create(-1, "/");

	// Flush the tail segment to disk
	flush_tail_segment_to_disk();
	return 0;
}
