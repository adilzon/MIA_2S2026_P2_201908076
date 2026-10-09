#include "../lib/users.h"

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstring>

using namespace std;

Users::Users() {}

string Users::read_users_file(string path, Structs::Partition partition) {
    FILE *rfile = fopen(path.c_str(), "rb");
    if (rfile == NULL) {
        throw runtime_error("no se pudo abrir el disco para leer users.txt");
    }

    Structs::Superblock super;
    fseek(rfile, partition.part_start, SEEK_SET);
    fread(&super, sizeof(Structs::Superblock), 1, rfile);

    Structs::Inodes inode1;
    fseek(rfile, super.s_inode_start + sizeof(Structs::Inodes), SEEK_SET);
    fread(&inode1, sizeof(Structs::Inodes), 1, rfile);

    string content = "";
    int remaining = inode1.i_size;

    for (int b = 0; b < 12 && remaining > 0; b++) {
        if (inode1.i_block[b] != -1) {
            Structs::Fileblock fb;
            fseek(rfile, super.s_block_start + (inode1.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
            fread(&fb, sizeof(Structs::Fileblock), 1, rfile);
            int to_read = min(remaining, 64);
            content.append(fb.b_content, to_read);
            remaining -= to_read;
        }
    }

    if (remaining > 0 && inode1.i_block[12] != -1) {
        Structs::Pointerblock pb;
        fseek(rfile, super.s_block_start + (inode1.i_block[12] * sizeof(Structs::Folderblock)), SEEK_SET);
        fread(&pb, sizeof(Structs::Pointerblock), 1, rfile);
        for (int p = 0; p < 16 && remaining > 0; p++) {
            if (pb.b_pointers[p] != -1) {
                Structs::Fileblock fb;
                fseek(rfile, super.s_block_start + (pb.b_pointers[p] * sizeof(Structs::Folderblock)), SEEK_SET);
                fread(&fb, sizeof(Structs::Fileblock), 1, rfile);
                int to_read = min(remaining, 64);
                content.append(fb.b_content, to_read);
                remaining -= to_read;
            }
        }
    }

    fclose(rfile);
    return content;
}

void Users::write_users_file(string path, Structs::Partition partition, string new_content) {
    FILE *wfile = fopen(path.c_str(), "rb+");
    if (wfile == NULL) {
        throw runtime_error("no se pudo abrir el disco para escribir users.txt");
    }

    Structs::Superblock super;
    fseek(wfile, partition.part_start, SEEK_SET);
    fread(&super, sizeof(Structs::Superblock), 1, wfile);

    Structs::Inodes inode1;
    fseek(wfile, super.s_inode_start + sizeof(Structs::Inodes), SEEK_SET);
    fread(&inode1, sizeof(Structs::Inodes), 1, wfile);

    int num_blocks = (new_content.size() + 63) / 64;
    if (num_blocks == 0) num_blocks = 1;

    // Asignar bloques directos si faltan
    for (int b = 0; b < min(num_blocks, 12); b++) {
        if (inode1.i_block[b] == -1) {
            // Buscar bloque libre en bitmap
            fseek(wfile, super.s_bm_block_start, SEEK_SET);
            char ch;
            int free_b = -1;
            for (int k = 0; k < super.s_blocks_count; k++) {
                fread(&ch, 1, 1, wfile);
                if (ch == '0') {
                    free_b = k;
                    break;
                }
            }
            if (free_b == -1) {
                fclose(wfile);
                throw runtime_error("no hay bloques libres para users.txt");
            }
            // Marcar en bitmap
            fseek(wfile, super.s_bm_block_start + free_b, SEEK_SET);
            char one = '1';
            fwrite(&one, 1, 1, wfile);
            super.s_free_blocks_count--;
            inode1.i_block[b] = free_b;
        }
    }

    // Escribir contenido por bloques
    for (int b = 0; b < num_blocks; b++) {
        string chunk = new_content.substr(b * 64, 64);
        Structs::Fileblock fb;
        memset(fb.b_content, 0, sizeof(fb.b_content));
        memcpy(fb.b_content, chunk.data(), chunk.size());

        int blk_idx = inode1.i_block[b];
        fseek(wfile, super.s_block_start + (blk_idx * sizeof(Structs::Folderblock)), SEEK_SET);
        fwrite(&fb, sizeof(Structs::Fileblock), 1, wfile);
    }

    inode1.i_size = new_content.size();
    inode1.i_mtime = time(nullptr);

    // Guardar inodo y superblock
    fseek(wfile, super.s_inode_start + sizeof(Structs::Inodes), SEEK_SET);
    fwrite(&inode1, sizeof(Structs::Inodes), 1, wfile);

    fseek(wfile, partition.part_start, SEEK_SET);
    fwrite(&super, sizeof(Structs::Superblock), 1, wfile);

    fclose(wfile);
}

vector<string> Users::getElements(string txt, char c) {
    vector<string> v;
    string line;
    stringstream ss(txt);
    while (getline(ss, line, c)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty() || c == ',') {
            v.push_back(line);
        }
    }
    return v;
}

bool Users::login(vector<string> context, Mount m) {
    mount = m;
    string id = "";
    string usuario = "";
    string password = "";

    for (auto current : context) {
        string id_ = shared.lower(current.substr(0, current.find('=')));
        current.erase(0, id_.length() + 1);
        if (current.front() == '\"') {
            current = current.substr(1, current.length() - 2);
        }
        if (shared.compare(id_, "id")) {
            id = current;
        } else if (shared.compare(id_, "usuario") || shared.compare(id_, "user") || shared.compare(id_, "usr")) {
            usuario = current;
        } else if (shared.compare(id_, "password") || shared.compare(id_, "pass") || shared.compare(id_, "pwd")) {
            password = current;
        }
    }

    if (id.empty() || usuario.empty() || password.empty()) {
        shared.handler("LOGIN", "requiere parámetros obligatorios (-user, -pass, -id)");
        return false;
    }
    return login(usuario, password, id);
}

bool Users::login(string u, string p, string id) {
    try {
        string path;
        Structs::Partition partition = mount.getmount(id, &path);

        string txt = read_users_file(path, partition);
        vector<string> lines = getElements(txt, '\n');

        for (const string &line : lines) {
            if (line.empty()) continue;
            vector<string> fields = getElements(line, ',');
            if (fields.size() >= 5 && (fields[1] == "U" || fields[1] == "u")) {
                int uid = stoi(fields[0]);
                if (uid != 0) { // Usuario activo
                    if (fields[3] == u && fields[4] == p) {
                        logged.id = id;
                        logged.user = u;
                        logged.password = p;
                        logged.uid = uid;
                        logged.grp = fields[2];
                        shared.response("LOGIN", "sesion iniciada correctamente para: " + u);
                        return true;
                    }
                }
            }
        }
        throw runtime_error("credenciales incorrectas");
    }
    catch (exception &e) {
        shared.handler("LOGIN", e.what());
        return false;
    }
}

bool Users::logout() {
    shared.response("LOGOUT", "hasta luego " + logged.user);
    logged = User();
    return false;
}

void Users::grp(vector<string> context, string action) {
    vector<string> required = {"name"};
    string name;

    for (int i = 0; i < (int)context.size(); i++) {
        string current = context.at(i);
        string id_ = current.substr(0, current.find('='));
        current.erase(0, id_.length() + 1);
        if (current.front() == '\"') {
            current = current.substr(1, current.length() - 2);
        }

        if (shared.compare(id_, "name")) {
            auto itr = find(required.begin(), required.end(), "name");
            if (itr != required.end()) required.erase(itr);
            name = current;
        }
    }

    if (!required.empty()) {
        shared.handler(action + "GRP", "requiere parámetro obligatorio -name");
        return;
    }

    if (shared.compare(action, "MK")) {
        mkgrp(name);
    } else {
        rmgrp(name);
    }
}

void Users::mkgrp(string n) {
    try {
        if (!shared.compare(logged.user, "root")) {
            throw runtime_error("solo el usuario root puede crear grupos");
        }
        string path;
        Structs::Partition partition = mount.getmount(logged.id, &path);

        string txt = read_users_file(path, partition);
        vector<string> lines = getElements(txt, '\n');

        int max_gid = 0;
        for (const string &line : lines) {
            if (line.empty()) continue;
            vector<string> fields = getElements(line, ',');
            if (fields.size() >= 3 && (fields[1] == "G" || fields[1] == "g")) {
                int gid = stoi(fields[0]);
                if (gid > max_gid) max_gid = gid;
                if (gid != 0 && fields[2] == n) {
                    throw runtime_error("el grupo ya existe");
                }
            }
        }

        txt += to_string(max_gid + 1) + ",G," + n + "\n";
        write_users_file(path, partition, txt);
        shared.response("MKGRP", "grupo creado correctamente: " + n);
    }
    catch (exception &e) {
        shared.handler("MKGRP", e.what());
    }
}

void Users::rmgrp(string n) {
    try {
        if (!shared.compare(logged.user, "root")) {
            throw runtime_error("solo el usuario root puede eliminar grupos");
        }
        if (n == "root") {
            throw runtime_error("no se puede eliminar el grupo root");
        }
        string path;
        Structs::Partition partition = mount.getmount(logged.id, &path);

        string txt = read_users_file(path, partition);
        vector<string> lines = getElements(txt, '\n');

        bool found = false;
        string new_txt = "";
        for (const string &line : lines) {
            if (line.empty()) continue;
            vector<string> fields = getElements(line, ',');
            if (fields.size() >= 3 && (fields[1] == "G" || fields[1] == "g")) {
                int gid = stoi(fields[0]);
                if (gid != 0 && fields[2] == n) {
                    found = true;
                    new_txt += "0,G," + n + "\n";
                    continue;
                }
            }
            new_txt += line + "\n";
        }

        if (!found) {
            throw runtime_error("el grupo no existe");
        }

        write_users_file(path, partition, new_txt);
        shared.response("RMGRP", "grupo eliminado correctamente: " + n);
    }
    catch (exception &e) {
        shared.handler("RMGRP", e.what());
    }
}

void Users::usr(vector<string> context, string action) {
    vector<string> required;
    if (shared.compare(action, "MK")) {
        required = {"user", "pass", "grp"};
    } else {
        required = {"user"};
    }
    string usr;
    string pwd;
    string grp;

    for (int i = 0; i < (int)context.size(); i++) {
        string current = context.at(i);
        string id_ = current.substr(0, current.find('='));
        current.erase(0, id_.length() + 1);
        if (current.front() == '\"') {
            current = current.substr(1, current.length() - 2);
        }

        if (shared.compare(id_, "usr") || shared.compare(id_, "user") || shared.compare(id_, "usuario")) {
            auto itr = find(required.begin(), required.end(), "user");
            if (itr != required.end()) required.erase(itr);
            usr = current;
        } else if (shared.compare(id_, "pwd") || shared.compare(id_, "pass") || shared.compare(id_, "password")) {
            auto itr = find(required.begin(), required.end(), "pass");
            if (itr != required.end()) required.erase(itr);
            pwd = current;
        } else if (shared.compare(id_, "grp") || shared.compare(id_, "grupo")) {
            auto itr = find(required.begin(), required.end(), "grp");
            if (itr != required.end()) required.erase(itr);
            grp = current;
        }
    }

    if (!required.empty()) {
        shared.handler(action + "USR", "requiere parámetros obligatorios");
        return;
    }

    if (shared.compare(action, "MK")) {
        mkusr(usr, pwd, grp);
    } else {
        rmusr(usr);
    }
}

void Users::mkusr(string usr, string pwd, string grp) {
    try {
        if (!shared.compare(logged.user, "root")) {
            throw runtime_error("solo el usuario root puede crear usuarios");
        }
        string path;
        Structs::Partition partition = mount.getmount(logged.id, &path);

        string txt = read_users_file(path, partition);
        vector<string> lines = getElements(txt, '\n');

        bool grp_exists = false;
        int max_uid = 0;

        for (const string &line : lines) {
            if (line.empty()) continue;
            vector<string> fields = getElements(line, ',');
            if (fields.size() >= 3 && (fields[1] == "G" || fields[1] == "g")) {
                int gid = stoi(fields[0]);
                if (gid != 0 && fields[2] == grp) {
                    grp_exists = true;
                }
            } else if (fields.size() >= 5 && (fields[1] == "U" || fields[1] == "u")) {
                int uid = stoi(fields[0]);
                if (uid > max_uid) max_uid = uid;
                if (uid != 0 && fields[3] == usr) {
                    throw runtime_error("el usuario ya existe");
                }
            }
        }

        if (!grp_exists) {
            throw runtime_error("el grupo especificado no existe");
        }

        txt += to_string(max_uid + 1) + ",U," + grp + "," + usr + "," + pwd + "\n";
        write_users_file(path, partition, txt);
        shared.response("MKUSR", "usuario creado correctamente: " + usr);
    }
    catch (exception &e) {
        shared.handler("MKUSR", e.what());
    }
}

void Users::rmusr(string usr) {
    try {
        if (!shared.compare(logged.user, "root")) {
            throw runtime_error("solo el usuario root puede eliminar usuarios");
        }
        if (usr == "root") {
            throw runtime_error("no se puede eliminar el usuario root");
        }
        string path;
        Structs::Partition partition = mount.getmount(logged.id, &path);

        string txt = read_users_file(path, partition);
        vector<string> lines = getElements(txt, '\n');

        bool found = false;
        string new_txt = "";
        for (const string &line : lines) {
            if (line.empty()) continue;
            vector<string> fields = getElements(line, ',');
            if (fields.size() >= 5 && (fields[1] == "U" || fields[1] == "u")) {
                int uid = stoi(fields[0]);
                if (uid != 0 && fields[3] == usr) {
                    found = true;
                    new_txt += "0,U," + fields[2] + "," + fields[3] + "," + fields[4] + "\n";
                    continue;
                }
            }
            new_txt += line + "\n";
        }

        if (!found) {
            throw runtime_error("el usuario no existe");
        }

        write_users_file(path, partition, new_txt);
        shared.response("RMUSR", "usuario eliminado correctamente: " + usr);
    }
    catch (exception &e) {
        shared.handler("RMUSR", e.what());
    }
}

void Users::chgrp_cmd(vector<string> context) {
    vector<string> required = {"user", "grp"};
    string user;
    string grp;

    for (int i = 0; i < (int)context.size(); i++) {
        string current = context.at(i);
        string id_ = current.substr(0, current.find('='));
        current.erase(0, id_.length() + 1);
        if (current.front() == '\"') {
            current = current.substr(1, current.length() - 2);
        }

        if (shared.compare(id_, "usr") || shared.compare(id_, "user") || shared.compare(id_, "usuario")) {
            auto itr = find(required.begin(), required.end(), "user");
            if (itr != required.end()) required.erase(itr);
            user = current;
        } else if (shared.compare(id_, "grp") || shared.compare(id_, "grupo")) {
            auto itr = find(required.begin(), required.end(), "grp");
            if (itr != required.end()) required.erase(itr);
            grp = current;
        }
    }

    if (!required.empty()) {
        shared.handler("CHGRP", "requiere parámetros obligatorios (-user, -grp)");
        return;
    }
    chgrp(user, grp);
}

void Users::chgrp(string usr, string grp) {
    try {
        if (!shared.compare(logged.user, "root")) {
            throw runtime_error("solo el usuario root puede cambiar de grupo");
        }
        string path;
        Structs::Partition partition = mount.getmount(logged.id, &path);

        string txt = read_users_file(path, partition);
        vector<string> lines = getElements(txt, '\n');

        bool grp_exists = false;
        bool user_exists = false;

        for (const string &line : lines) {
            if (line.empty()) continue;
            vector<string> fields = getElements(line, ',');
            if (fields.size() >= 3 && (fields[1] == "G" || fields[1] == "g")) {
                int gid = stoi(fields[0]);
                if (gid != 0 && fields[2] == grp) grp_exists = true;
            } else if (fields.size() >= 5 && (fields[1] == "U" || fields[1] == "u")) {
                int uid = stoi(fields[0]);
                if (uid != 0 && fields[3] == usr) user_exists = true;
            }
        }

        if (!user_exists) throw runtime_error("el usuario no existe");
        if (!grp_exists) throw runtime_error("el grupo no existe");

        string new_txt = "";
        for (const string &line : lines) {
            if (line.empty()) continue;
            vector<string> fields = getElements(line, ',');
            if (fields.size() >= 5 && (fields[1] == "U" || fields[1] == "u")) {
                int uid = stoi(fields[0]);
                if (uid != 0 && fields[3] == usr) {
                    new_txt += fields[0] + ",U," + grp + "," + fields[3] + "," + fields[4] + "\n";
                    continue;
                }
            }
            new_txt += line + "\n";
        }

        write_users_file(path, partition, new_txt);
        shared.response("CHGRP", "grupo del usuario " + usr + " cambiado a " + grp);
    }
    catch (exception &e) {
        shared.handler("CHGRP", e.what());
    }
}
