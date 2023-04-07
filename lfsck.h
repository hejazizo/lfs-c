void print_segment_summary_blocks();
void in_use_inodes_with_no_data();
void useless_entries();
int inode_address_conflict_with_ifile(int inum_from_segment_summary,int segment_no,int block_no);
int indirect_address_conflict_with_inode(int inum_from_segment_summary,int segment_no,int block_no);
int data_address_conflict_with_inode(int inum_from_segment_summary,int segment_no,int block_no);