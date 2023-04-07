#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/xattr.h>

#include "log.h"
#include "file.h"
#include "directory.h"

#include <unistd.h>

int current_dir_inum = 1;

char **parse_path(char *str,int *number_of_elements)
{
    for (int i = 0; i < strlen(str); ++i)
    {
        if (str[i] == '/')
        {
            (*number_of_elements)++;
        }
    }

    char **result = (char **)calloc(*number_of_elements,sizeof(char *));
    int i_1=1;
    int iterator = 0;

    if (strcmp( str, "/" ) == 0)
    {
        *number_of_elements = 0;
        return NULL;
    }

    for (int i = 1; i < strlen(str); ++i)
    {
        if (str[i] == '/')
        {
            result[iterator] = (char *)calloc(i-i_1,sizeof(char));
            strncpy(result[iterator],str+i_1,(i-i_1)*sizeof(char));
            // printf("%s\n", result[iterator]);
            iterator++;
            i_1 = i+1;
        }
        else if (i == strlen(str)-1)
        {
            result[iterator] = (char *)calloc(i-i_1+1,sizeof(char));
            strncpy(result[iterator],str+i_1,(i-i_1+1)*sizeof(char));
            // printf("%s\n", result[iterator]);
        }

    }

    return result;
}

static int lfs_mkdir(const char *path, mode_t mode)
{
    printf("******************************** In MKDIR with path %s ****************************\n", path);
    (void) mode;

    int *number_of_directory_files = (int *)calloc(1,sizeof(int));
    char **directory_file_names = parse_path(path,number_of_directory_files);

    if (*number_of_directory_files == 1)
    {
        int inum_parent = 1;
        directory_create(inum_parent,directory_file_names[(*number_of_directory_files)-1]);
        
        return 0;
    }
    else if (*number_of_directory_files>1)
    {
        int inum_parent = get_inum_by_path(directory_file_names,(*number_of_directory_files)-1);
        directory_create(inum_parent,directory_file_names[(*number_of_directory_files)-1]);

        return 0;
    }
    else
    {
        return -ENOENT;
    }
    
}

static int lfs_rmdir(const char *path)
{
    printf("******************************** In RMDIR with path %s ****************************\n", path);
    int *number_of_directory_files = (int *)calloc(1,sizeof(int));
    char **directory_file_names = parse_path(path,number_of_directory_files);

    if (strcmp( path, "/" ) == 0)
    {
        printf("Cannot remove root.\n");
        return -ENOENT;
    }
    else if (*number_of_directory_files >= 1)
    {
        int inum = get_inum_by_path(directory_file_names,(*number_of_directory_files));
        // printf(">><< inum to delete: %d\n",inum );
        if (inum == -1)
        {
            return -ENOENT;
        }
        else
        {
            directory_remove(inum);

            return 0;
        }
    }
    else
    {
        return -ENOENT;
    }
    
}

static int lfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{

    (void) fi;

    printf("******************************** In READDIR with path %s ****************************\n", path);

    if ( strcmp( path, "/" ) == 0 )
    {
        int *dir_count = (int *)calloc(1,sizeof(int));
        directory_entry **entries = get_subdirectories(1,dir_count);
        // printf(">>><<< dir count for %s : %d\n", *dir_count, path);

        for (int i = 0; i < *dir_count; ++i)
        {
            // printf(">>><<< %s\n",entries[i]->name );
            filler(buffer,entries[i]->name,NULL,0);
            
        }
        return 0;
    }
    else
    {
        int *number_of_directory_files = (int *)calloc(1,sizeof(int));
        char **directory_file_names = parse_path(path,number_of_directory_files);
        int inum = get_inum_by_path(directory_file_names,(*number_of_directory_files));

        if (inum == -1)
        {
            return -ENOENT;
        }
        else if(get_file_type(inum) == 2)
        {
            return -ENOENT;
        }
        else
        {
            int *dir_count = (int *)calloc(1,sizeof(int));
            directory_entry **entries = get_subdirectories(inum,dir_count);
            // printf(">>><<< dir count for %s : %d\n", *dir_count, path);
            for (int i = 0; i < *dir_count; ++i)
            {   
                // printf(">>><<< %s\n",entries[i]->name );
                filler(buffer,entries[i]->name,NULL,0);
            }
            return 0;
        }
    }
}

static int lfs_getattr(const char *path, struct stat *stbuf)
{
    printf("******************************** In GETATTR with path %s ****************************\n", path);
    if (strcmp(path, "/") == 0) 
    {
        printf("ROOT DIR\n");
        // root directory
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = time( NULL );
        stbuf->st_mtime = time( NULL );

        return 0;
    }
    else
    {
        int *number_of_directory_files = (int *)calloc(1,sizeof(int));
        char **directory_file_names = parse_path(path,number_of_directory_files);
        int inum = get_inum_by_path(directory_file_names,(*number_of_directory_files));
        printf("inum: %d\n", inum);
        
        if(inum == -1)
        {
            return -ENOENT;
        }
        else
        {
            stbuf->st_uid = getuid();
            stbuf->st_gid = getgid();
            stbuf->st_atime = time( NULL );
            stbuf->st_mtime = time( NULL );

            int file_type = get_file_type(inum);
            
            if (file_type == 1)
            {
                printf("DIR\n");
                // directory
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;

                return 0;
            }
            else
            {
                printf("FILE\n");
                // file
                stbuf->st_mode = S_IFREG | 0666;
                stbuf->st_nlink = 2;
                stbuf->st_size = get_file_size(inum);

                return 0;
            }
        }
    }
    
}

static int lfs_mknod(const char* path, mode_t mode, dev_t rdev)
{
    printf("******************************** In MKNOD with path %s ****************************\n", path);
    (void) mode;
    (void) rdev;

    int *number_of_directory_files = (int *)calloc(1,sizeof(int));
    char **directory_file_names = parse_path(path,number_of_directory_files);

    if (strcmp( path, "/" ) == 0)
    {
        return -ENOENT;
    }
    else
    {
        int inum_parent = 1;

        if (*number_of_directory_files > 1)
        {
            inum_parent = get_inum_by_path(directory_file_names,(*number_of_directory_files)-1);
        }

        int inum = file_create(2);
        add_child_entry_to_parent_directory(inum_parent,directory_file_names[(*number_of_directory_files)-1],inum);

        return 0;

    }
}

static int lfs_unlink(const char *path)
{
    printf("******************************** In UNLINK with path %s ****************************\n", path);
    if (strcmp(path, "/") == 0) 
    {
        
        printf("Can't delete the root directory.\n");
        return 0;
    }
    else
    {
        int *number_of_directory_files = (int *)calloc(1,sizeof(int));
        char **directory_file_names = parse_path(path,number_of_directory_files);
        int inum = get_inum_by_path(directory_file_names,(*number_of_directory_files));

        if(inum == -1)
        {
            printf("File not found.\n");
            return -ENOENT;
        }
        else
        {
            int file_type = get_file_type(inum);
            
            if (file_type == 1)
            {
                
                printf("Cannot remove. This is a directory.\n");
                return -ENOENT;
            }
            else
            {
                int parent_inum = 1;

                if (*number_of_directory_files > 1)
                {
                    parent_inum = get_inum_by_path(directory_file_names,(*number_of_directory_files)-1);
                }

                // printf("parent inum: %d , self inum: %d\n", parent_inum,inum );
                file_remove(inum,parent_inum);

                return 0;
            }
        }
    }
}

static int lfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    printf("******************************** In READ with path %s ****************************\n", path);
    (void) fi;

    int *number_of_directory_files = (int *)calloc(1,sizeof(int));
    char **directory_file_names = parse_path(path,number_of_directory_files);

    if (strcmp( path, "/" ) == 0)
    {
        return -ENOENT;
    }
    else
    {
        int inum = get_inum_by_path(directory_file_names,*number_of_directory_files);
        if (inum == -1)
        {
            return -ENOENT;
        }
        else if (get_file_type(inum) == 1)
        {
            printf("It is a directory not a file.\n");
            return -ENOENT;
        }
        i_node *current_inode = get_inode(inum);
        file_read(inum,offset,(int)size,(void *)buf);

        return (int)size;
    }

}

static int lfs_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    printf("******************************** In WRITE with path %s ****************************\n", path);
    (void) fi;

    int *number_of_directory_files = (int *)calloc(1,sizeof(int));
    char **directory_file_names = parse_path(path,number_of_directory_files);

    if (strcmp( path, "/" ) == 0)
    {
        printf("Root directory is not a file.\n");
        return -ENOENT;
    }
    else
    {
        int inum = get_inum_by_path(directory_file_names,*number_of_directory_files);
        if (inum == -1)
        {
            printf("File not found.\n");
            return -ENOENT;
        }
        else if (get_file_type(inum) == 1)
        {
            printf("It is a directory not a file.\n");
            return -ENOENT;
        }

        file_write(inum,offset,(int)size,(void *)buf);

        return get_file_size(inum);
    }

}

static int lfs_flush (const char *path , struct fuse_file_info *fi)
{
    printf("******************************** In FLUSH with path %s ****************************\n", path);
    (void) path;
    (void) fi;
    
    return 0; 
}

static int lfs_open(const char *path, struct fuse_file_info *fi)
{
    printf("******************************** In OPEN with path %s ****************************\n", path);
    (void) path;
    (void) fi;
    
    if (strcmp(path, "/") == 0) 
    {
        printf("Can't open root directory.\n");
        return -ENOENT;
    }
    else
    {
        int *number_of_directory_files = (int *)calloc(1,sizeof(int));
        char **directory_file_names = parse_path(path,number_of_directory_files);
        int inum = get_inum_by_path(directory_file_names,(*number_of_directory_files));

        if(inum == -1)
        {
            printf("File not found.\n");
            return -ENOENT;
        }
        else
        {
            int file_type = get_file_type(inum);
            
            if (file_type == 1)
            {
                printf("Can't open a directory.\n");

                return -ENOENT;
            }
            else
            {
                return 0;
            }
        }
    }

    return -ENOENT;
}

static int lfs_truncate(const char *path, off_t size)
{
    printf("******************************** In TRUNCATE with path %s ****************************\n", path);
    (void) path;
    (void) size;
    
    return 0;
}

static struct fuse_operations lfs_oper = {
    .mkdir	 = lfs_mkdir,
    .getattr = lfs_getattr,
    .readdir = lfs_readdir,
    .rmdir   = lfs_rmdir,
    .mknod   = lfs_mknod,
    .unlink  = lfs_unlink,
    .read    = lfs_read,
    .write   = lfs_write,
    .open    = lfs_open,
    .flush   = lfs_flush,
    .truncate = lfs_truncate,
};

int main(int argc, char *argv[])
{
    if (argc<3)
    {
        printf("Number of arguments is not enough. \n");
        return -1;
    }

    char *file_name = argv[argc-2];
    char *mount_point = argv[argc-1];
    int cache_size = 0;

    int argc_new = 1; //file name
    char **argv_new = (char **)calloc(5,sizeof(char *));
    for (int i = 0; i < 5; ++i)
    {
        argv_new[i] = (char *)calloc(1,sizeof(50));
    }

    strcpy(argv_new[0],argv[0]);

    for (int i = 1; i < argc-1; ++i)
    {
        if(strcmp(argv[i],"-f")==0)
        {
            strcpy(argv_new[argc_new],"-f");
            argc_new++;
        }
        else if(strcmp(argv[i],"-s")==0)
        {
            cache_size =  atoi(argv[i+1]);
        }
        else if(strcmp(argv[i],"-i")==0)
        {
            // check point interval
        }
        else if(strcmp(argv[i],"-c")==0)
        {
            // threshold cleaning start
        }
        else if(strcmp(argv[i],"-C")==0)
        {
            // threshold cleaning stops
        }
    }

    strcpy(argv_new[argc_new],"-d");
    argc_new++;
    strcpy(argv_new[argc_new],"-s");
    argc_new++;
    
    strcpy(argv_new[argc_new],mount_point);
    argc_new++;

    Log_Initialize(file_name,cache_size);

    return fuse_main(argc_new, argv_new, &lfs_oper, NULL);
}
