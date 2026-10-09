#include "../lib/mount.h"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>

using namespace std;

Mount::Mount() {
    for (int i = 0; i < 99; i++) {
        mounted[i].status = '0';
        mounted[i].path[0] = '\0';
        mounted[i].letter = 'A';
        for (int j = 0; j < 26; j++) {
            mounted[i].mpartitions[j].status = '0';
            mounted[i].mpartitions[j].name[0] = '\0';
            mounted[i].mpartitions[j].id[0] = '\0';
            mounted[i].mpartitions[j].num = 0;
            mounted[i].mpartitions[j].letter = 'A';
        }
    }
}

void Mount::mount(vector<string> context) {
    if (context.empty()) {
        listmount();
        return;
    }
    vector<string> required = {"name", "path"};
    string path;
    string name;

    for (auto current : context) {
        string id = shared.lower(current.substr(0, current.find('=')));
        current.erase(0, id.length() + 1);
        if (current.front() == '\"') {
            current = current.substr(1, current.length() - 2);
        }

        if (shared.compare(id, "name")) {
            if (count(required.begin(), required.end(), id)) {
                auto itr = find(required.begin(), required.end(), id);
                required.erase(itr);
            }
            name = current;
        } else if (shared.compare(id, "path")) {
            if (count(required.begin(), required.end(), id)) {
                auto itr = find(required.begin(), required.end(), id);
                required.erase(itr);
            }
            path = current;
        }
    }
    if (!required.empty()) {
        shared.handler("MOUNT", "parametros obligatorios faltantes (-path, -name)");
        return;
    }
    mount(path, name);
}

void Mount::mount(string p, string n) {
    try {
        FILE *validate = fopen(p.c_str(), "rb");
        if (validate == NULL) {
            throw runtime_error("el disco no existe en la ruta: " + p);
        }

        Structs::MBR disk;
        rewind(validate);
        fread(&disk, sizeof(Structs::MBR), 1, validate);
        fclose(validate);

        Structs::Partition partition = dsk.findby(disk, n, p);
        if (partition.part_status != '1') {
            throw runtime_error("partición no existente: " + n);
        }

        if (partition.part_type == 'E' || partition.part_type == 'e') {
            vector<Structs::EBR> ebrs = dsk.getlogics(partition, p);
            if (!ebrs.empty()) {
                Structs::EBR ebr = ebrs.at(0);
                n = ebr.part_name;
            } else {
                throw runtime_error("no se puede montar una partición extendida");
            }
        }

        // Check if partition is already mounted
        for (int i = 0; i < 99; i++) {
            if (mounted[i].status == '1') {
                for (int j = 0; j < 26; j++) {
                    if (mounted[i].mpartitions[j].status == '1') {
                        if (shared.compare(mounted[i].path, p) && shared.compare(mounted[i].mpartitions[j].name, n)) {
                            throw runtime_error("la partición " + n + " ya está montada");
                        }
                    }
                }
            }
        }

        // Search for existing disk
        int disk_idx = -1;
        for (int i = 0; i < 99; i++) {
            if (mounted[i].status == '1' && shared.compare(mounted[i].path, p)) {
                disk_idx = i;
                break;
            }
        }

        // If new disk, find empty slot and determine letter
        if (disk_idx == -1) {
            char used_letters[26] = {0};
            for (int i = 0; i < 99; i++) {
                if (mounted[i].status == '1') {
                    char l = toupper(mounted[i].letter);
                    if (l >= 'A' && l <= 'Z') {
                        used_letters[l - 'A'] = 1;
                    }
                }
            }
            char next_letter = 'A';
            for (int l = 0; l < 26; l++) {
                if (!used_letters[l]) {
                    next_letter = 'A' + l;
                    break;
                }
            }

            for (int i = 0; i < 99; i++) {
                if (mounted[i].status == '0') {
                    disk_idx = i;
                    mounted[i].status = '1';
                    strncpy(mounted[i].path, p.c_str(), sizeof(mounted[i].path) - 1);
                    mounted[i].letter = next_letter;
                    break;
                }
            }
        }

        if (disk_idx == -1) {
            throw runtime_error("límite máximo de discos montados alcanzado");
        }

        // Find partition slot in disk
        for (int j = 0; j < 26; j++) {
            if (mounted[disk_idx].mpartitions[j].status == '0') {
                mounted[disk_idx].mpartitions[j].status = '1';
                mounted[disk_idx].mpartitions[j].num = j + 1;
                mounted[disk_idx].mpartitions[j].letter = mounted[disk_idx].letter;
                strncpy(mounted[disk_idx].mpartitions[j].name, n.c_str(), sizeof(mounted[disk_idx].mpartitions[j].name) - 1);
                
                string id_str = "76" + to_string(j + 1) + string(1, mounted[disk_idx].letter);
                strncpy(mounted[disk_idx].mpartitions[j].id, id_str.c_str(), sizeof(mounted[disk_idx].mpartitions[j].id) - 1);
                
                shared.response("MOUNT", "se ha realizado correctamente el mount -id=" + id_str);
                return;
            }
        }

        throw runtime_error("límite máximo de particiones montadas alcanzado en este disco");
    }
    catch (exception &e) {
        shared.handler("MOUNT", e.what());
    }
}

void Mount::unmount(vector<string> context) {
    vector<string> required = {"id"};
    string id_;

    for (int i = 0; i < (int)context.size(); i++) {
        string current = context.at(i);
        string id = current.substr(0, current.find('='));
        current.erase(0, id.length() + 1);
        if (current.front() == '\"') {
            current = current.substr(1, current.length() - 2);
        }

        if (shared.compare(id, "id")) {
            auto itr = find(required.begin(), required.end(), id);
            if (itr != required.end()) required.erase(itr);
            id_ = current;
        }
    }
    if (!required.empty()) {
        shared.handler("UNMOUNT", "falta parámetro obligatorio -id");
        return;
    }
    unmount(id_);
}

void Mount::unmount(string id) {
    try {
        for (int i = 0; i < 99; i++) {
            if (mounted[i].status == '1') {
                for (int j = 0; j < 26; j++) {
                    if (mounted[i].mpartitions[j].status == '1') {
                        if (shared.compare(mounted[i].mpartitions[j].id, id)) {
                            mounted[i].mpartitions[j].status = '0';
                            mounted[i].mpartitions[j].id[0] = '\0';
                            mounted[i].mpartitions[j].name[0] = '\0';
                            shared.response("UNMOUNT", "se ha realizado correctamente el unmount -id=" + id);
                            return;
                        }
                    }
                }
            }
        }
        throw runtime_error("ID no encontrado: " + id);
    }
    catch (exception &e) {
        shared.handler("UNMOUNT", e.what());
    }
}

Structs::Partition Mount::getmount(string id, string *p) {
    for (int i = 0; i < 99; i++) {
        if (mounted[i].status == '1') {
            for (int j = 0; j < 26; j++) {
                if (mounted[i].mpartitions[j].status == '1') {
                    if (shared.compare(mounted[i].mpartitions[j].id, id)) {
                        FILE *validate = fopen(mounted[i].path, "rb");
                        if (validate == NULL) {
                            throw runtime_error("disco no existente en la ruta: " + string(mounted[i].path));
                        }
                        Structs::MBR disk;
                        fread(&disk, sizeof(Structs::MBR), 1, validate);
                        fclose(validate);
                        *p = string(mounted[i].path);
                        return dsk.findby(disk, mounted[i].mpartitions[j].name, mounted[i].path);
                    }
                }
            }
        }
    }
    throw runtime_error("ID no encontrado: " + id);
}

void Mount::listmount() {
    cout << "\n<-------------------------- MOUNTS -------------------------->" << endl;
    bool found = false;
    for (int i = 0; i < 99; i++) {
        if (mounted[i].status == '1') {
            for (int j = 0; j < 26; j++) {
                if (mounted[i].mpartitions[j].status == '1') {
                    found = true;
                    cout << "> " << mounted[i].mpartitions[j].id << ", "
                         << mounted[i].mpartitions[j].name << ", "
                         << mounted[i].path << endl;
                }
            }
        }
    }
    if (!found) {
        cout << "No hay particiones montadas." << endl;
    }
    cout << "<------------------------------------------------------------>\n" << endl;
}
