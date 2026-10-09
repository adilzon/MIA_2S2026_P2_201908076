#ifndef MOUNT_H
#define MOUNT_H

#include <string>
#include <vector>
#include "../lib/shared.h"
#include "../lib/structs.h"
#include "../lib/disco.h"

using namespace std;

class Mount {
public:
    Mount();

    typedef struct _MP
    {
        char letter = 'A';
        int num = 0;
        char status = '0';
        char name[30] = "";
        char id[15] = "";
    } MountedPartition;

    typedef struct _MD
    {
        char path[260] = "";
        char status = '0';
        char letter = 'A';
        MountedPartition mpartitions[26];
    } MountedDisc;

    MountedDisc mounted[99];

    void mount(vector<string> context);
    void unmount(vector<string> context);
    void mount(string p, string n);
    void unmount(string id);
    void listmount();
    Structs::Partition getmount(string id, string *p);

private:
    Disk dsk;
    Shared shared;
};

#endif