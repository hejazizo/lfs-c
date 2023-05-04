#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "directory.h"
#include <time.h>
#include "file.h"
#include <math.h>
#include <string.h>



/**
* Removes a directory from the file system.
*
* @param dir_inum The inode number of the directory to remove
*
* @return 0 on success, -1 on failure
*/

int directory_remove(int dir_inum)
{
	// Get the subdirectories of the directory to be removed
	int *subdir_count = (int *)calloc(1, sizeof(int));
	DirectoryEntry **subdirs = get_subdirectories(dir_inum, subdir_count);

	// Count the number of swap files in the directory
	int swap_file_count = 0;
	for (int i = 0; i < *subdir_count; ++i)
	{
		if (strstr(subdirs[i]->name, ".swp") != NULL)
		{
			swap_file_count++;
		}
	}

	// If the directory contains subdirectories, do not remove it
	if ((*subdir_count) - swap_file_count > 2)
	{
		printf("This directory contains subdirectories. \n");
		free(subdirs);
		free(subdir_count);
		return -1;
	}

	// Remove the directory from the parent directory list
	for (int i = 0; i < *subdir_count; ++i)
	{
		if (subdirs[i]->name[0] == '.' && subdirs[i]->name[1] == '.')
		{
			remove_child_entry_from_parent_directory(subdirs[i]->i_num, dir_inum);
		}
	}

	// Remove the directory from the ifile
	Log_Remove_mapping(dir_inum);

	// Free the memory allocated for the directory entries and count
	free(subdirs);
	free(subdir_count);

	// Return 0 to indicate success
	return 0;
}


/**
* Removes a file from the file system.
*
* @param file_num The inode number of the file to remove
* @param parent_dir_inum inode number of the parent directory of the file
*
* @return 0 on success
*/
int file_remove(int file_inum, int parent_dir_inum)
{
	// Remove the file from the parent directory entries
	remove_child_entry_from_parent_directory(parent_dir_inum, file_inum);

	// Free the inode of the file
	free_file(file_inum);

	// Return 0 to indicate success
	return 0;
}



/**
* Creates a new directory in the file system.
*
* @param parent_dir_inum The inode number of the parent directory of the new directory
* @param dir_name The name of the new directory
*
* @return The inode number of the newly created directory
*/

int directory_create(int parent_dir_inum, char *dir_name)
{
	// Create a special file for storing the information inside the directory
	int my_inum = create_file(1);

	DirectoryEntry entries[2];

	strcpy(entries[0].name, ".");
	entries[0].i_num = my_inum;

	// The second entry represents the parent directory
	strcpy(entries[1].name, "..");
	entries[1].i_num = parent_dir_inum;

	if (parent_dir_inum != -1)
	{
		add_child_entry_to_parent_directory(parent_dir_inum, dir_name, my_inum);
	}

	// Write the entries to the file
	write_to_file(my_inum, 0, sizeof(entries), entries);

	// Return the inode number of the newly created directory
	return my_inum;
}


/**
* Removes a child directory entry from its parent directory.
*
* @param parent_inum The inode number of the parent directory
* @param child_inum The inode number of the child directory to remove
*
* @return 0 on success, -1 on failure
*/
int remove_child_entry_from_parent_directory(int parent_inum, int child_inum)
{
    // Retrieve the inode of the parent directory
    i_node* parent_inode = get_inode(parent_inum);
    if (parent_inode == NULL) {
        printf("Cannot remove child %d from parent %d. Parent inode not found.\n", child_inum, parent_inum);
        return -1;
    }

    // Allocate a buffer to read the parent directory data
    void* buffer = calloc(parent_inode->eof_index_in_bytes + 1, sizeof(char));
    if (buffer == NULL) {
        printf("Error: failed to allocate buffer.\n");
        return -1;
    }

    // Read the parent directory data into the buffer
    if (read_file_from_start_to_end(parent_inum, buffer) < 0) {
        printf("Error: failed to read parent directory data.\n");
        free(buffer);
        return -1;
    }

    // Calculate the number of directory entries in the parent directory
    int entry_count = (parent_inode->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

    // Iterate through all the directory entries in the parent directory
    for (int i = 0; i < entry_count; ++i) {
        // Retrieve the current directory entry
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + (i * sizeof(DirectoryEntry)));

        // If the current directory entry is the child entry, remove it
        if (entry->i_num == child_inum) {
            // Move all subsequent directory entries back by one index
            memmove(buffer + (i * sizeof(DirectoryEntry)), buffer + ((i + 1) * sizeof(DirectoryEntry)), (entry_count - i - 1) * sizeof(DirectoryEntry));
            // Update the parent directory's eof_index_in_bytes value
            parent_inode->eof_index_in_bytes -= sizeof(DirectoryEntry);
            // Write the updated parent directory data back to the disk
            if (write_to_file(parent_inum, 0, parent_inode->eof_index_in_bytes + 1, buffer) < 0) {
                printf("Error: failed to write parent directory data.\n");
                free(buffer);
                return -1;
            }
            free(buffer);
            return 0;
        }
    }
	printf("Cannot remove child %d from parent %d. Child entry not found.\n", child_inum, parent_inum);
    free(buffer);
    return -1;
}

    // If the child entry is not found, return failure



/**
* Prints the contents of a directory.
*
* @param inum The inode number of the directory to print
*/

void print_new_entries(int inum)
{
    // Get the inode of the directory
    i_node *inode = get_inode(inum);

    // If inode is not found, return failure
    if (inode->i_num == -1)
    {
        printf("Directory inode not found.\n");
        return;
    }

    // Allocate a buffer to read the directory data
    void *buffer = calloc(inode->eof_index_in_bytes + 1, sizeof(char));

    // Read the directory data into the buffer
    read_file_from_start_to_end(inum, buffer);

    // Calculate the number of directory entries in the directory
    int entry_count = (inode->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

    // Create an array to store the directory entries
    DirectoryEntry entries[entry_count];

    // Copy all directory entries from buffer to entries array
    memcpy(entries, buffer, entry_count * sizeof(DirectoryEntry));

    // Iterate through all the directory entries in the directory
    for (int i = 0; i < entry_count; ++i)
    {
        // Print the inode number and name of the current directory entry
        printf("inum: %d name: %s\n", entries[i].i_num, entries[i].name);
    }

    // Free the buffer
    free(buffer);
}



/**
* Adds a child directory entry to a parent directory.
*
* @param parent_inum The inode number of the parent directory
* @param child_name The name of the child directory
* @param child_inum The inode number of the child directory
*
* @return 0 on success
*/

int add_child_entry_to_parent_directory(int parent_inum, char *child_name, int child_inum)
{
// Get the inode of the parent directory
i_node *parent_inode = get_inode(parent_inum);

// Allocate a buffer to read the parent directory data
void *buffer = (void *)calloc(parent_inode->eof_index_in_bytes + 1, sizeof(char));

// Read the parent directory data into the buffer
read_file_from_start_to_end(parent_inum, buffer);

// Calculate the number of directory entries in the parent directory
int entry_count = (parent_inode->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

// Allocate memory for the new directory entry
DirectoryEntry *new_entry = (DirectoryEntry *)malloc(sizeof(DirectoryEntry));

// Initialize the new directory entry with the child inode number and name
new_entry->i_num = child_inum;
strcpy(new_entry->name, child_name);

// Allocate memory for the updated directory entries
DirectoryEntry *entries = (DirectoryEntry *)malloc((entry_count + 1) * sizeof(DirectoryEntry));

// Copy the existing directory entries from the buffer to the entries array
memcpy(entries, buffer, entry_count * sizeof(DirectoryEntry));

// Add the new directory entry to the end of the entries array
memcpy(&entries[entry_count], new_entry, sizeof(DirectoryEntry));

// Write the updated directory entries to the parent directory
write_to_file(parent_inum, 0, (entry_count + 1) * sizeof(DirectoryEntry), entries);

// Free dynamically allocated memory
free(new_entry);
free(entries);
free(buffer);

// Return 0 to indicate success
return 0;
}



/**
* Retrieves the subdirectories of a directory.
*
* @param dir_inum The inode number of the directory
* @param entry_count Pointer to an integer that will store the number of subdirectories found
*
* @return An array of pointers to directory entries, or NULL if the directory does not exist
*/

DirectoryEntry** get_subdirectories(int dir_inum, int* entry_count) {
// Get the inode of the directory
i_node* dir_inode = get_inode(dir_inum);


// Return NULL if the directory does not exist
if (dir_inode->i_num == -1) {
    return NULL;
}

// Allocate a buffer to read the directory data
void* buffer = (void*)calloc(dir_inode->eof_index_in_bytes + 1, sizeof(char));

// Read the directory data into the buffer
read_file_from_start_to_end(dir_inum, buffer);

// Calculate the number of directory entries in the directory
*entry_count = (dir_inode->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

// Allocate an array of pointers to directory entries
DirectoryEntry** entries = (DirectoryEntry**)calloc(*entry_count, sizeof(DirectoryEntry*));

// Copy each directory entry from the buffer to the entries array
for (int i = 0; i < *entry_count; ++i) {
    entries[i] = (DirectoryEntry*)calloc(1, sizeof(DirectoryEntry));
    memcpy(entries[i], buffer + (i * sizeof(DirectoryEntry)), sizeof(DirectoryEntry));
}

// Free the buffer
free(buffer);

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
    int *dir_count_ptr = (int *)calloc(1, sizeof(int));
    DirectoryEntry **entries_ptr = get_subdirectories(1, dir_count_ptr);

    // Return -1 if the root directory does not exist
    if (entries_ptr == NULL)
    {
        return -1;
    }

    // Traverse the directory hierarchy to find the file or directory
    for (int i = 0; i < number_elements; ++i)
    {
        // Search through each directory entry in the current directory
        for (int j = 0; j < *dir_count_ptr; ++j)
        {
            // If the name of the directory entry matches the next element in the path
            if (strcmp(entries_ptr[j]->name, directory_file_elements[i]) == 0)
            {
                // If we have reached the end of the path, return the inode number of the file or directory
                if (i == number_elements - 1)
                {
                    return entries_ptr[j]->i_num;
                }
                // Otherwise, get the subdirectories of the current directory and continue traversing
                else
                {
                    free(dir_count_ptr);
                    dir_count_ptr = (int *)calloc(1, sizeof(int));
                    free(entries_ptr);
                    entries_ptr = get_subdirectories(entries_ptr[j]->i_num, dir_count_ptr);
                    break;
                }
            }
        }
    }

    // If the file or directory was not found, return -1
    return -1;
}
