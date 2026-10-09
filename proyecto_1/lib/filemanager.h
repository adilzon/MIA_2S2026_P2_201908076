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

private:
    int allocate_inode(FILE *disk, Structs::Superblock &sb, char type, int perm, int uid, int gid, int size);
    int allocate_block(FILE *disk, Structs::Superblock &sb);
    bool add_folder_entry(FILE *disk, Structs::Superblock &sb, int parent_inode_idx, string name, int child_inode_idx);

    Mount mount;
    Shared shared;
};

#endif // END OF DECLARATION