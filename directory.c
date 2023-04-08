#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "directory.h"
#include <time.h>
#include "file.h"
#include <math.h>
#include <string.h>

/**
* Creates a new directory in the file system.
*
* @param parent_directory_inum The inode number of the parent directory of the new directory
* @param directory_name The name of the new directory
*
* @return The inode number of the newly created directory
*/
int directory_create(int parent_directory_inum, char *directory_name)
{
	// Create a special file for storing the information inside the directory
	int my_inum = file_create(1);

	// Create entries for the directory
	DirectoryEntry entries[2];
	// The first entry represents the directory itself
	strcpy(entries[0].name, ".");
	entries[0].i_num = my_inum;
	// The second entry represents the parent directory
	strcpy(entries[1].name, "..");
	entries[1].i_num = parent_directory_inum;

	// Write the entries to the file
	file_write(my_inum, 0, sizeof(entries), &entries);

	// If the directory is not the root directory, update the parent directory entries
	if (parent_directory_inum != -1)
	{
		add_child_entry_to_parent_directory(parent_directory_inum, directory_name, my_inum);
	}

	// Return the inode number of the newly created directory
	return my_inum;
}



/**
* Removes a directory from the file system.
*
* @param inum The inode number of the directory to remove
*
* @return 0 on success, -1 on failure
*/
int directory_remove(int inum)
{
	// Get the subdirectories of the directory to be removed
	int *dir_count = (int *)calloc(1, sizeof(int));
	DirectoryEntry **entries = get_subdirectories(inum, dir_count);

	// Count the number of swap files in the directory
	int swap_files = 0;
	for (int i = 0; i < *dir_count; ++i)
	{
		if (strstr(entries[i]->name, ".swp") != NULL)
		{
			swap_files++;
		}
	}

	// If the directory contains subdirectories, do not remove it
	if ((*dir_count) - swap_files > 2)
	{
		printf("This directory contains subdirectories. \n");
		free(entries);
		free(dir_count);
		return -1;
	}

	// Remove the directory from the parent directory list
	for (int i = 0; i < *dir_count; ++i)
	{
		if (entries[i]->name[0] == '.' && entries[i]->name[1] == '.')
		{
			remove_child_entry_from_parent_directory(entries[i]->i_num, inum);
		}
	}

	// Remove the directory from the ifile
	Log_Remove_mapping(inum);

	// Free the memory allocated for the directory entries and count
	free(entries);
	free(dir_count);

	// Return 0 to indicate success
	return 0;
}


/**
* Removes a file from the file system.
*
* @param inum The inode number of the file to remove
* @param parent_inum The inode number of the parent directory of the file
*
* @return 0 on success
*/
int file_remove(int inum, int parent_inum)
{
	// Remove the file from the parent directory entries
	remove_child_entry_from_parent_directory(parent_inum, inum);

	// Free the inode of the file
	file_free(inum);

	// Return 0 to indicate success
	return 0;
}



/**
* Removes a child directory entry from its parent directory.
*
* @param inum The inode number of the parent directory
* @param child_inum The inode number of the child directory to remove
*
* @return 0 on success, -1 on failure
*/
int remove_child_entry_from_parent_directory(int inum, int child_inum)
{
	// Get the inode of the parent directory
	i_node *tmp = get_inode(inum);

	// If the parent inode is not found, return failure
	if (tmp->i_num == -1)
	{
		printf("Can't remove %d from its parent %d. parent inode not found.\n", child_inum, inum);
		return -1;
	}

	// Allocate a buffer to read the parent directory data
	void *buffer = (void *)calloc(tmp->eof_index_in_bytes + 1, sizeof(char));

	// Read the parent directory data into the buffer
	file_read_begining_to_end(inum, buffer);

	// Calculate the number of directory entries in the parent directory
	int entry_count = (tmp->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

	// Create an array to store the directory entries after removing the child entry
	DirectoryEntry entries[entry_count - 1];

	// Initialize a variable to keep track of the index shift when removing the child entry
	int index_correcter = 0;

	// Iterate through all the directory entries in the parent directory
	for (int i = 0; i < entry_count; ++i)
	{
		// Allocate a temporary directory entry structure to read the current directory entry
		DirectoryEntry *tmp_entry = (DirectoryEntry *)calloc(1, sizeof(DirectoryEntry));

		// Copy the current directory entry from the buffer to the temporary structure
		memcpy(tmp_entry, buffer + (i * sizeof(DirectoryEntry)), sizeof(DirectoryEntry));

		// If the current directory entry is not the child entry, add it to the entries array
		if (tmp_entry->i_num != child_inum)
		{
			entries[i - index_correcter].i_num = tmp_entry->i_num;
			strcpy(entries[i - index_correcter].name, tmp_entry->name);
		}
		// If the current directory entry is the child entry, adjust the index_correcter variable
		else
		{
			index_correcter = 1;
		}
	}

	// Write the updated directory entries to the parent directory
	file_write(inum, 0, sizeof(entries), (void *)&entries);

	// Return success
	return 0;
}


/**
* Prints the contents of a directory.
*
* @param inum The inode number of the directory to print
*/
void print_new_entries(int inum)
{
	// Get the inode of the directory
	i_node *tmp = get_inode(inum);

	// Allocate a buffer to read the directory data
	void *buffer = (void *)calloc(tmp->eof_index_in_bytes + 1, sizeof(char));

	// Read the directory data into the buffer
	file_read_begining_to_end(inum, buffer);

	// Calculate the number of directory entries in the directory
	int entry_count = (tmp->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

	// Create an array to store the directory entries
	DirectoryEntry entries[entry_count];

	// Iterate through all the directory entries in the directory
	for (int i = 0; i < entry_count; ++i)
	{
		// Copy the current directory entry from the buffer to the entries array
		memcpy(&entries[i], buffer + (i * sizeof(DirectoryEntry)), sizeof(DirectoryEntry));

		// Print the inode number and name of the current directory entry
		printf("inum: %d name: %s\n", entries[i].i_num, entries[i].name);
	}
}


/**
* Adds a child directory entry to a parent directory.
*
* @param inum The inode number of the parent directory
* @param child_name The name of the child directory
* @param child_inum The inode number of the child directory
*
* @return 0 on success
*/
int add_child_entry_to_parent_directory(int inum, char *child_name, int child_inum)
{
	// Get the inode of the parent directory
	i_node *tmp = get_inode(inum);

	// Allocate a buffer to read the parent directory data
	void *buffer = (void *)calloc(tmp->eof_index_in_bytes + 1, sizeof(char));

	// Read the parent directory data into the buffer
	file_read_begining_to_end(inum, buffer);

	// Calculate the number of directory entries in the parent directory
	int entry_count = (tmp->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

	// Create an array to store the directory entries, with space for the new child entry
	DirectoryEntry entries[entry_count + 1];

	// Copy the existing directory entries from the buffer to the entries array
	for (int i = 0; i < entry_count; ++i)
	{
		memcpy(&entries[i], buffer + (i * sizeof(DirectoryEntry)), sizeof(DirectoryEntry));
	}

	// Create a new directory entry for the child directory
	DirectoryEntry last_entry;
	strcpy(last_entry.name, child_name);
	last_entry.i_num = child_inum;

	// Add the new directory entry to the end of the entries array
	memcpy(&entries[entry_count], &last_entry, sizeof(DirectoryEntry));

	// Write the updated directory entries to the parent directory
	file_write(inum, 0, sizeof(entries), &entries);

	// Return 0 to indicate success
	return 0;
}


/**
* Retrieves the subdirectories of a directory.
*
* @param inum The inode number of the directory
* @param entry_count Pointer to an integer that will store the number of subdirectories found
*
* @return An array of pointers to directory entries, or NULL if the directory does not exist
*/
DirectoryEntry **get_subdirectories(int inum, int *entry_count)
{
	// Get the inode of the directory
	i_node *tmp = get_inode(inum);

	// Return NULL if the directory does not exist
	if (tmp->i_num == -1)
	{
		return NULL;
	}

	// Allocate a buffer to read the directory data
	void *buffer = (void *)calloc(tmp->eof_index_in_bytes + 1, sizeof(char));

	// Read the directory data into the buffer
	file_read_begining_to_end(inum, buffer);

	// Calculate the number of directory entries in the directory
	*entry_count = (tmp->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

	// Create an array of pointers to directory entries
	DirectoryEntry **entries = (DirectoryEntry **)calloc(*entry_count, sizeof(DirectoryEntry *));

	// Copy each directory entry from the buffer to the entries array
	for (int i = 0; i < *entry_count; ++i)
	{
		entries[i] = (DirectoryEntry *)calloc(1, sizeof(DirectoryEntry));
		memcpy(entries[i], buffer + (i * sizeof(DirectoryEntry)), sizeof(DirectoryEntry));
	}

	// Return the array of pointers to directory entries
	return entries;
}


/**
* Gets the inode number of a file or directory given its path.
*
* @param directory_file_elements An array of strings representing the path to the file or directory
* @param number_elements The number of elements in the path
*
* @return The inode number of the file or directory, or -1 if it does not exist
*/
int get_inum_by_path(char **directory_file_elements, int number_elements)
{
	// Get the subdirectories of the root directory
	int *dir_count = (int *)calloc(1, sizeof(int));
	DirectoryEntry **entries = get_subdirectories(1, dir_count);

	// Return -1 if the root directory does not exist
	if (entries == NULL)
	{
		return -1;
	}

	// Traverse the directory hierarchy to find the file or directory
	for (int i = 0; i < number_elements; ++i)
	{
		// Search through each directory entry in the current directory
		for (int j = 0; j < *dir_count; ++j)
		{
			// If the name of the directory entry matches the next element in the path
			if (strcmp(entries[j]->name, directory_file_elements[i]) == 0)
			{
				// If we have reached the end of the path, return the inode number of the file or directory
				if (i == number_elements - 1)
				{
					return entries[j]->i_num;
				}
				// Otherwise, get the subdirectories of the current directory and continue traversing
				else
				{
					dir_count = (int *)calloc(1, sizeof(int));
					entries = get_subdirectories(entries[j]->i_num, dir_count);
					break;
				}
			}
		}
	}

	// If the file or directory was not found, return -1
	return -1;
}
