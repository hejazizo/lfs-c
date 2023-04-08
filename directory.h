typedef struct DirectoryEntry
{
    char name[50];
    int i_num;
} DirectoryEntry;

int directory_create(int parent_directory_inum, char *directory_name);
DirectoryEntry **get_subdirectories(int inum, int *entry_count);
int get_inum_by_path(char **directory_file_elements, int number_elements);
int add_child_entry_to_parent_directory(int inum, char *child_name, int child_inum);
int directory_remove(int inum);
int remove_child_entry_from_parent_directory(int inum, int child_inum);
int file_remove(int inum, int parent_inum);
void print_new_entries(int inum);
