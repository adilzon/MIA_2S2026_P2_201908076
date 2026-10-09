#ifndef ADMIN_H
#define ADMIN_H

#include <string>
#include <vector>
#include "../lib/shared.h"
#include "../lib/structs.h"
#include "../lib/mount.h"

using namespace std;

class Users {
public:
    Users();

    typedef struct _User {
        string user;
        string password;
        string id;
        int uid = -1;
        int gid = -1;
        string grp;
    } User;

    bool login(vector<string> context, Mount m);
    bool login(string u, string p, string id);
    bool logout();

    void grp(vector<string> context, string action);
    void usr(vector<string> context, string action);
    void chgrp_cmd(vector<string> context);

    void mkgrp(string n);
    void rmgrp(string n);
    void mkusr(string usr, string pwd, string grp);
    void rmusr(string usr);
    void chgrp(string usr, string grp);

    string read_users_file(string path, Structs::Partition partition);
    void write_users_file(string path, Structs::Partition partition, string new_content);
    vector<string> getElements(string txt, char c);

    User logged;

private:
    Shared shared;
    Mount mount;
};

#endif // END OF DECLARATION