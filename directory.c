#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "directory.h"
#include <time.h>
#include "file.h"
#include <math.h>
#include <string.h>

int directory_create(int parent_directory_inum,char *directory_name)
{
	// create a special file for keeping the information inside the directory
	int my_inum = file_create(1);
	
	// creating the usual entries of each directory (self and root)
	directory_entry entries[2];
	strcpy(entries[0].name,".");
	entries[0].i_num = my_inum;
	strcpy(entries[1].name,"..");
	entries[1].i_num = parent_directory_inum;

	// write entries to the file
	file_write(my_inum,0,sizeof(entries),&entries);

	// update parent entries
	// if root directory then has no parent
	if (parent_directory_inum != -1)
	{
		add_child_entry_to_parent_directory(parent_directory_inum,directory_name,my_inum);
	}

	// Flush_Tail_Segment_To_Disk();
	return my_inum;
}

int directory_remove(int inum)
{
	int *dir_count = (int *)calloc(1,sizeof(int));
    directory_entry **entries = get_subdirectories(inum,dir_count);

    int swap_files = 0;
    for (int i = 0; i < *dir_count; ++i)
	{
		if (strstr(entries[i]->name,".swp")!=NULL)
		{
			swap_files++;
		}
	}

	// printf("number swaps: %d\n", swap_files);

    if ((*dir_count)-swap_files>2)
    {
    	printf("This directory contains subdirectory. \n");
    	
    	return -1;
    }

    // remove from parent directory list
    for (int i = 0; i < *dir_count; ++i)
    {
    	if (entries[i]->name[0]=='.' && entries[i]->name[1]=='.')
    	{
    		remove_child_entry_from_parent_directory(entries[i]->i_num,inum);
    	}
    }

    // remove from ifile
    Log_Remove_mapping(inum);

    return 0;
}

int file_remove(int inum, int parent_inum)
{
	// remove from parent entries
	remove_child_entry_from_parent_directory(parent_inum,inum);

    // remove from ifile
    file_free(inum);

    return 0;
}

int remove_child_entry_from_parent_directory(int inum, int child_inum)
{
	i_node *tmp = get_inode(inum);
	
	if (tmp->i_num ==-1)
	{
		printf("Can't remove %d from its parent %d. parent inode not found.\n", child_inum,inum);
		return -1;
	}

	void *buffer = (void *)calloc(tmp->eof_index_in_bytes+1,sizeof(char));
	file_read_begining_to_end(inum,buffer);
	
	int entry_count = (tmp->eof_index_in_bytes+1)/sizeof(directory_entry);
	directory_entry entries[entry_count-1]; 

	int index_correcter = 0;
	for (int i = 0; i < entry_count; ++i)
	{
		directory_entry *tmp_entry = (directory_entry *)calloc(1,sizeof(directory_entry));

		memcpy(tmp_entry,buffer+(i*sizeof(directory_entry)),sizeof(directory_entry));
		
		if ( tmp_entry->i_num != child_inum )
		{
			// memcpy(&entries[i],buffer+(i*sizeof(directory_entry)),sizeof(directory_entry));
			entries[i-index_correcter].i_num = tmp_entry->i_num;
			strcpy(entries[i-index_correcter].name,tmp_entry->name);
		}
		else
		{
			index_correcter = 1;
		}
	}
	
	file_write(inum,0,sizeof(entries),(void *)&entries);

	return 0;
}

void print_new_entries(int inum)
{
	i_node *tmp = get_inode(inum);
	void *buffer = (void *)calloc(tmp->eof_index_in_bytes+1,sizeof(char));
	file_read_begining_to_end(inum,buffer);

	int entry_count = (tmp->eof_index_in_bytes+1)/sizeof(directory_entry);
	directory_entry entries[entry_count]; 

	for (int i = 0; i < entry_count; ++i)
	{
		memcpy(&entries[i],buffer+(i*sizeof(directory_entry)),sizeof(directory_entry));
		printf("inum: %d name: %s\n",entries[i].i_num,entries[i].name );
	}

}

int add_child_entry_to_parent_directory(int inum,char *child_name, int child_inum)
{
	i_node *tmp = get_inode(inum);
	void *buffer = (void *)calloc(tmp->eof_index_in_bytes+1,sizeof(char));
	file_read_begining_to_end(inum,buffer);

	int entry_count = (tmp->eof_index_in_bytes+1)/sizeof(directory_entry);
	directory_entry entries[entry_count+1]; 

	for (int i = 0; i < entry_count; ++i)
	{
		memcpy(&entries[i],buffer+(i*sizeof(directory_entry)),sizeof(directory_entry));
	}

	directory_entry last_entry;
	strcpy(last_entry.name,child_name);
	last_entry.i_num = child_inum;

	memcpy(&entries[entry_count],&last_entry,sizeof(directory_entry));

	file_write(inum,0,sizeof(entries),&entries);

	return 0;
}

directory_entry** get_subdirectories(int inum, int *entry_count)
{
	i_node *tmp = get_inode(inum);
	if (tmp->i_num == -1)
	{
		return NULL;
	}

	void *buffer = (void *)calloc(tmp->eof_index_in_bytes+1,sizeof(char));
	file_read_begining_to_end(inum,buffer);

	*entry_count = (tmp->eof_index_in_bytes+1)/sizeof(directory_entry);
	directory_entry **entries = (directory_entry **)calloc(*entry_count,sizeof(directory_entry *));

	for (int i = 0; i < *entry_count; ++i)
	{
		entries[i] = (directory_entry *)calloc(1,sizeof(directory_entry));
		memcpy(entries[i],buffer+(i*sizeof(directory_entry)),sizeof(directory_entry));
	}

	return entries;
}

int get_inum_by_path(char **directory_file_elements, int number_elements)
{
	int *dir_count = (int *)calloc(1,sizeof(int));
    directory_entry **entries = get_subdirectories(1,dir_count);
    if (entries == NULL)
    {
    	return -1;
    }

	for (int i = 0; i < number_elements; ++i)
	{
		for (int j = 0; j < *dir_count; ++j)
	    {
	        if (strcmp(entries[j]->name,directory_file_elements[i])==0)
	        {
	        	if (i==number_elements-1)
	        	{
	        		return entries[j]->i_num;
	        	}
	        	else
	        	{
				    dir_count = (int *)calloc(1,sizeof(int));
			    	entries = get_subdirectories(entries[j]->i_num,dir_count);
			    	break;
	        	}
	        }
	    }

	}

	return -1;
}



