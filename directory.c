#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "directory.h"
#include <time.h>
#include "file.h"
#include <math.h>
#include <string.h>

// This function delete a directory from the file system
//The inode number of the directory that you want to remove.
//Returns 0 if the directory was removed successfully, or -1 if the directory could not be removed.
int directory_remove(int dir_inum)
{
	// Get the subdirectories of the directory to be removed
	int *subdir_count = (int *)calloc(1, sizeof(int));
	DirectoryEntry **subdirs = get_subdirectories(dir_inum, subdir_count);

	// Count the number of swap files in the directory
	int swap_file_count = 0;
    int i = 0;
    while (i < *subdir_count)
    {
        if (strstr(subdirs[i]->name, ".swp") != NULL)
        {
            swap_file_count++;
        }
        i++;
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
    i = 0;
    while (i < *subdir_count)
    {
        if (subdirs[i]->name[0] == '.' && subdirs[i]->name[1] == '.')
        {
            remove_child_entry_from_parent_directory(subdirs[i]->i_num, dir_inum);
        }
        i++;
    }

	// Remove the directory from the ifile
	Log_Remove_mapping(dir_inum);

	// Free the memory allocated for the directory entries and count
	free(subdirs);
	free(subdir_count);

	// Return 0 to indicate success
	return 0;
}

//file_num: The inode number of the file that needs to be removed
//parent_dir_inum: The inode number of the directory that contains the file
// Returns 0 if the file was removed successfully

int file_remove(int file_inum, int parent_dir_inum)
{
	// Remove the file from the parent directory entries
	remove_child_entry_from_parent_directory(parent_dir_inum, file_inum);

	// Free the inode of the file
	free_file(file_inum);

	// Return 0 to indicate success
	return 0;
}

//this function creates a new directory in the file system
//parent_dir_inum: The inode number of the parent directory where the new directory will be created
//dir_name: The name of the new directory
//The function returns the inode number of the newly created directory
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

//This function removes a child directory entry from its parent directory
// It takes the inode number of the parent directory as "parent_inum" and
// the inode number of the child directory to be removed as "child_inum"
// The function returns 0 on success and -1 on failure
int remove_child_entry_from_parent_directory(int parent_inum, int child_inum)
{
    // Retrieve the inode of the parent directory
    i_node* dir_parent_inode = get_inode(parent_inum);
    if (dir_parent_inode == NULL) {
        printf("Cannot remove child %d from parent %d. Parent inode not found.\n", child_inum, parent_inum);
        return -1;
    }

    // Allocate a buffer to read the parent directory data
    void* buffer = calloc(dir_parent_inode->eof_index_in_bytes + 1, sizeof(char));
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
    int entry_count = (dir_parent_inode->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

    // Iterate through all the directory entries in the parent directory
    for (int i = 0; i < entry_count; ++i) {
        // Retrieve the current directory entry
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + (i * sizeof(DirectoryEntry)));

        // If the current directory entry is the child entry, remove it
        if (entry->i_num == child_inum) {
            // Move all subsequent directory entries back by one index
            memmove(buffer + (i * sizeof(DirectoryEntry)), buffer + ((i + 1) * sizeof(DirectoryEntry)), (entry_count - i - 1) * sizeof(DirectoryEntry));
            // Update the parent directory's eof_index_in_bytes value
            dir_parent_inode->eof_index_in_bytes -= sizeof(DirectoryEntry);
            // Write the updated parent directory data back to the disk
            if (write_to_file(parent_inum, 0, dir_parent_inode->eof_index_in_bytes + 1, buffer) < 0) {
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


// This function prints the contents of a directory specified by its inode number
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
    int i = 0;
    while (i < entry_count) {
        // Print the inode number and name of the current directory entry
        printf("inum: %d name: %s\n", entries[i].i_num, entries[i].name);
        i++;
    }

    // Free the buffer
    free(buffer);
}


//This function adds a new child directory entry to a specified parent directory in the file system.
//parent_inum The inode number of the parent directory.
//child_name The name of the child directory to be added.
//child_inum The inode number of the child directory.
//Returns 0 upon successful completion of the operation.

int add_child_entry_to_parent_directory(int parent_inum, char *child_name, int child_inum)
{
// Get the inode of the parent directory
i_node *dir_parent_inode = get_inode(parent_inum);

// Allocate a buffer to read the parent directory data
void *buffer = (void *)calloc(dir_parent_inode->eof_index_in_bytes + 1, sizeof(char));

// Read the parent directory data into the buffer
read_file_from_start_to_end(parent_inum, buffer);

// Calculate the number of directory entries in the parent directory
int entry_count = (dir_parent_inode->eof_index_in_bytes + 1) / sizeof(DirectoryEntry);

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
//This function retrieves the list of subdirectories of a directory
//specified by its inode number.
//dir_inum The inode number of the directory to retrieve subdirectories from
//entry_count A pointer to an integer that will be used to store the number of subdirectories found
//An array of pointers to directory entries, or NULL if the specified directory does not exist
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

//This function retrieves the inode number of a file or directory by providing its path as an array of strings.
//The number of elements in the path is specified by number_elements.
//If the file or directory exists, it returns its inode number, otherwise it returns -1.

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
