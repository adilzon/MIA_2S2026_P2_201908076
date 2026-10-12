#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <string>
#include <vector>
#include "../lib/shared.h"
#include "../lib/structs.h"
#include "../lib/mount.h"
#include "../lib/users.h"

using namespace std;

class FileManager {
public:
    FileManager();

    void mkdir(vector<string> context, Structs::Partition partition, string pth, Users &user);
    void mkfile(vector<string> context, Structs::Partition partition, string pth, Users &user);
    void cat(vector<string> context, Structs::Partition partition, string pth, Users &user);

    vector<string> getpath(string s);
    int getfree(Structs::Superblock spr, string pth, string t);
    void updatebm(Structs::Superblock spr, string pth, string t);

    int find_inode_by_path(FILE *disk, Structs::Superblock &sb, const vector<string> &path_parts);
    string read_file_content(FILE *disk, Structs::Superblock &sb, int inode_idx);

    void add_journal_entry(FILE *disk, const Structs::Partition &partition, const Structs::Superblock &sb,
                           const string &operation, const string &path, const string &content = "");
    static void record_journal(string disk_path, const Structs::Partition &partition,
                               const string &operation, const string &path, const string &content = "");
    void remove_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user);
    void rename_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user);
    void copy_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user);
    void move_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user);
    void find_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user);
    void chown_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user);
    void loss_cmd(vector<string> context, Mount &mount_inst);

    bool check_permission(const Structs::Inodes &inode, const Users::User &logged, int req_perm);
    bool check_permission_recursive(FILE *disk, Structs::Superblock &sb, int inode_idx, const Users::User &logged, int req_perm);
    void free_inode_and_blocks(FILE *disk, Structs::Superblock &sb, int inode_idx);
    bool remove_folder_entry(FILE *disk, Structs::Superblock &sb, int parent_inode_idx, string name);
    int copy_inode_recursive(FILE *disk_src, Structs::Superblock &sb_src, int src_inode_idx,
                             FILE *disk_dst, Structs::Superblock &sb_dst, int uid, int gid);
    bool match_pattern(const string &pattern, const string &text);
    void search_tree(FILE *disk, Structs::Superblock &sb, int inode_idx, const string &current_path,
                     const string &pattern, vector<string> &results);
    void chown_recursive(FILE *disk, Structs::Superblock &sb, int inode_idx, int new_uid);

private:
    int allocate_inode(FILE *disk, Structs::Superblock &sb, char type, int perm, int uid, int gid, int size);
    int allocate_block(FILE *disk, Structs::Superblock &sb);
    bool add_folder_entry(FILE *disk, Structs::Superblock &sb, int parent_inode_idx, string name, int child_inode_idx);

    Mount mount;
    Shared shared;
};

#endif // END OF DECLARATION