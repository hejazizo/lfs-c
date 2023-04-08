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
	initialize_log_system("flash.dat", 0);

	directory_create(1, "a");
	directory_create(1, "b");
	directory_create(1, "c");
	printf("three directory created.\n");
	print_new_entries(1);
	directory_remove(3);
	print_new_entries(1);

	return 0;
}
