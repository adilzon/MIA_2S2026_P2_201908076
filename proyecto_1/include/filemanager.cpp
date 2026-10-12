#include "../lib/filemanager.h"

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <cmath>

using namespace std;

FileManager::FileManager() {}

vector<string> FileManager::getpath(string s) {
    vector<string> result;
    if (s.empty()) return result;

    if (s.front() == '\"') {
        s = s.substr(1, s.length() - 2);
    }

    stringstream ss(s);
    string token;
    while (getline(ss, token, '/')) {
        if (!token.empty() && token != ".") {
            result.push_back(token);
        }
    }
    return result;
}

int FileManager::allocate_inode(FILE *disk, Structs::Superblock &sb, char type, int perm, int uid, int gid, int size) {
    fseek(disk, sb.s_bm_inode_start, SEEK_SET);
    char ch;
    int free_ino = -1;
    for (int i = 0; i < sb.s_inodes_count; i++) {
        fread(&ch, 1, 1, disk);
        if (ch == '0') {
            free_ino = i;
            break;
        }
    }

    if (free_ino == -1) {
        throw runtime_error("no hay inodos libres en el sistema de archivos");
    }

    // Marcar en bitmap de inodos
    fseek(disk, sb.s_bm_inode_start + free_ino, SEEK_SET);
    char one = '1';
    fwrite(&one, 1, 1, disk);
    sb.s_free_inodes_count--;

    // Inicializar inodo
    Structs::Inodes inode;
    inode.i_uid = uid;
    inode.i_gid = gid;
    inode.i_size = size;
    inode.i_atime = time(nullptr);
    inode.i_ctime = time(nullptr);
    inode.i_mtime = time(nullptr);
    inode.i_type = type;
    inode.i_perm = perm;
    for (int b = 0; b < 15; b++) inode.i_block[b] = -1;

    fseek(disk, sb.s_inode_start + (free_ino * sizeof(Structs::Inodes)), SEEK_SET);
    fwrite(&inode, sizeof(Structs::Inodes), 1, disk);

    return free_ino;
}

int FileManager::allocate_block(FILE *disk, Structs::Superblock &sb) {
    fseek(disk, sb.s_bm_block_start, SEEK_SET);
    char ch;
    int free_blk = -1;
    for (int i = 0; i < sb.s_blocks_count; i++) {
        fread(&ch, 1, 1, disk);
        if (ch == '0') {
            free_blk = i;
            break;
        }
    }

    if (free_blk == -1) {
        throw runtime_error("no hay bloques libres en el sistema de archivos");
    }

    // Marcar en bitmap de bloques
    fseek(disk, sb.s_bm_block_start + free_blk, SEEK_SET);
    char one = '1';
    fwrite(&one, 1, 1, disk);
    sb.s_free_blocks_count--;

    return free_blk;
}

bool FileManager::add_folder_entry(FILE *disk, Structs::Superblock &sb, int parent_inode_idx, string name, int child_inode_idx) {
    Structs::Inodes parent_inode;
    fseek(disk, sb.s_inode_start + (parent_inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    fread(&parent_inode, sizeof(Structs::Inodes), 1, disk);

    // Buscar espacio en bloques directos asignados
    for (int b = 0; b < 12; b++) {
        if (parent_inode.i_block[b] != -1) {
            Structs::Folderblock folder;
            fseek(disk, sb.s_block_start + (parent_inode.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
            fread(&folder, sizeof(Structs::Folderblock), 1, disk);

            for (int k = 0; k < 4; k++) {
                if (folder.b_content[k].b_inodo == -1) {
                    folder.b_content[k].b_inodo = child_inode_idx;
                    memset(folder.b_content[k].b_name, 0, sizeof(folder.b_content[k].b_name));
                    strncpy(folder.b_content[k].b_name, name.c_str(), 11);
                    fseek(disk, sb.s_block_start + (parent_inode.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                    fwrite(&folder, sizeof(Structs::Folderblock), 1, disk);
                    return true;
                }
            }
        }
    }

    // Si no hay espacio, asignar nuevo bloque a la carpeta
    for (int b = 0; b < 12; b++) {
        if (parent_inode.i_block[b] == -1) {
            int new_blk = allocate_block(disk, sb);
            parent_inode.i_block[b] = new_blk;
            parent_inode.i_mtime = time(nullptr);

            fseek(disk, sb.s_inode_start + (parent_inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
            fwrite(&parent_inode, sizeof(Structs::Inodes), 1, disk);

            Structs::Folderblock new_folder;
            for (int k = 0; k < 4; k++) {
                new_folder.b_content[k].b_inodo = -1;
                memset(new_folder.b_content[k].b_name, 0, sizeof(new_folder.b_content[k].b_name));
            }
            new_folder.b_content[0].b_inodo = child_inode_idx;
            strncpy(new_folder.b_content[0].b_name, name.c_str(), 11);

            fseek(disk, sb.s_block_start + (new_blk * sizeof(Structs::Folderblock)), SEEK_SET);
            fwrite(&new_folder, sizeof(Structs::Folderblock), 1, disk);
            return true;
        }
    }

    throw runtime_error("límite de bloques directos alcanzado en la carpeta padre");
}

int FileManager::find_inode_by_path(FILE *disk, Structs::Superblock &sb, const vector<string> &path_parts) {
    int current_inode = 0; // raíz "/"

    for (const string &part : path_parts) {
        Structs::Inodes inode;
        fseek(disk, sb.s_inode_start + (current_inode * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&inode, sizeof(Structs::Inodes), 1, disk);

        if (inode.i_type != '0') {
            return -1; // No es una carpeta
        }

        bool found = false;
        // Buscar en bloques directos
        for (int b = 0; b < 12; b++) {
            if (inode.i_block[b] != -1) {
                Structs::Folderblock folder;
                fseek(disk, sb.s_block_start + (inode.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                fread(&folder, sizeof(Structs::Folderblock), 1, disk);

                for (int k = 0; k < 4; k++) {
                    if (folder.b_content[k].b_inodo != -1 && shared.compare(folder.b_content[k].b_name, part)) {
                        current_inode = folder.b_content[k].b_inodo;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        if (!found) return -1;
    }
    return current_inode;
}

string FileManager::read_file_content(FILE *disk, Structs::Superblock &sb, int inode_idx) {
    Structs::Inodes inode;
    fseek(disk, sb.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    fread(&inode, sizeof(Structs::Inodes), 1, disk);

    string content = "";
    int remaining = inode.i_size;

    for (int b = 0; b < 12 && remaining > 0; b++) {
        if (inode.i_block[b] != -1) {
            Structs::Fileblock fb;
            fseek(disk, sb.s_block_start + (inode.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
            fread(&fb, sizeof(Structs::Fileblock), 1, disk);
            int to_read = min(remaining, 64);
            content.append(fb.b_content, to_read);
            remaining -= to_read;
        }
    }

    if (remaining > 0 && inode.i_block[12] != -1) {
        Structs::Pointerblock pb;
        fseek(disk, sb.s_block_start + (inode.i_block[12] * sizeof(Structs::Folderblock)), SEEK_SET);
        fread(&pb, sizeof(Structs::Pointerblock), 1, disk);
        for (int p = 0; p < 16 && remaining > 0; p++) {
            if (pb.b_pointers[p] != -1) {
                Structs::Fileblock fb;
                fseek(disk, sb.s_block_start + (pb.b_pointers[p] * sizeof(Structs::Folderblock)), SEEK_SET);
                fread(&fb, sizeof(Structs::Fileblock), 1, disk);
                int to_read = min(remaining, 64);
                content.append(fb.b_content, to_read);
                remaining -= to_read;
            }
        }
    }

    return content;
}

void FileManager::mkdir(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        bool p = false;
        string path = "";

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "p")) {
                p = true;
            } else if (shared.compare(id, "path")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') {
                    current = current.substr(1, current.length() - 2);
                }
                path = current;
            }
        }

        if (path.empty()) {
            throw runtime_error("requiere parámetro obligatorio -path");
        }

        vector<string> path_parts = getpath(path);
        if (path_parts.empty()) {
            throw runtime_error("ruta de carpeta no válida");
        }

        FILE *disk = fopen(pth.c_str(), "rb+");
        if (disk == NULL) {
            throw runtime_error("no se pudo abrir el disco");
        }

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        int current_inode = 0; // Raíz

        for (int i = 0; i < (int)path_parts.size(); i++) {
            vector<string> sub_path(path_parts.begin(), path_parts.begin() + i + 1);
            int found_inode = find_inode_by_path(disk, sb, sub_path);

            if (found_inode != -1) {
                current_inode = found_inode;
            } else {
                // Si falta una carpeta intermedia y -p no está activado
                if (i < (int)path_parts.size() - 1 && !p) {
                    fclose(disk);
                    throw runtime_error("no existen las carpetas padres");
                }

                // Crear carpeta
                int new_ino = allocate_inode(disk, sb, '0', 664, user.logged.uid, 1, 0);
                int new_blk = allocate_block(disk, sb);

                // Configurar inodo de la nueva carpeta
                Structs::Inodes folder_inode;
                fseek(disk, sb.s_inode_start + (new_ino * sizeof(Structs::Inodes)), SEEK_SET);
                fread(&folder_inode, sizeof(Structs::Inodes), 1, disk);
                folder_inode.i_block[0] = new_blk;
                fseek(disk, sb.s_inode_start + (new_ino * sizeof(Structs::Inodes)), SEEK_SET);
                fwrite(&folder_inode, sizeof(Structs::Inodes), 1, disk);

                // Inicializar bloque con . y ..
                Structs::Folderblock fb;
                for (int k = 0; k < 4; k++) {
                    fb.b_content[k].b_inodo = -1;
                    memset(fb.b_content[k].b_name, 0, sizeof(fb.b_content[k].b_name));
                }
                strcpy(fb.b_content[0].b_name, ".");
                fb.b_content[0].b_inodo = new_ino;
                strcpy(fb.b_content[1].b_name, "..");
                fb.b_content[1].b_inodo = current_inode;

                fseek(disk, sb.s_block_start + (new_blk * sizeof(Structs::Folderblock)), SEEK_SET);
                fwrite(&fb, sizeof(Structs::Folderblock), 1, disk);

                // Añadir entrada a la carpeta padre
                add_folder_entry(disk, sb, current_inode, path_parts[i], new_ino);

                current_inode = new_ino;
            }
        }

        // Registrar en Journal si es EXT3
        if (sb.s_filesystem_type == 3) {
            add_journal_entry(disk, partition, sb, "mkdir", path, "-");
        }

        // Guardar Superblock
        fseek(disk, partition.part_start, SEEK_SET);
        fwrite(&sb, sizeof(Structs::Superblock), 1, disk);
        fclose(disk);

        shared.response("MKDIR", "directorio creado correctamente: " + path);
    }
    catch (exception &e) {
        shared.handler("MKDIR", e.what());
    }
}

void FileManager::mkfile(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        bool r = false;
        string path = "";
        int size = 0;
        string cont = "";
        bool has_size = false;

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "r")) {
                r = true;
            } else if (shared.compare(id, "path")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') {
                    current = current.substr(1, current.length() - 2);
                }
                path = current;
            } else if (shared.compare(id, "size")) {
                current.erase(0, id.length() + 1);
                size = stoi(current);
                has_size = true;
            } else if (shared.compare(id, "cont")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') {
                    current = current.substr(1, current.length() - 2);
                }
                cont = current;
            }
        }

        if (path.empty()) {
            throw runtime_error("requiere parámetro obligatorio -path");
        }
        if (size < 0) {
            throw runtime_error("el parámetro -size no puede ser negativo");
        }

        vector<string> path_parts = getpath(path);
        if (path_parts.empty()) {
            throw runtime_error("ruta de archivo no válida");
        }

        string filename = path_parts.back();
        path_parts.pop_back();

        FILE *disk = fopen(pth.c_str(), "rb+");
        if (disk == NULL) {
            throw runtime_error("no se pudo abrir el disco");
        }

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        // Navegar o crear directorios padres
        int current_inode = 0;
        for (int i = 0; i < (int)path_parts.size(); i++) {
            vector<string> sub_path(path_parts.begin(), path_parts.begin() + i + 1);
            int found_inode = find_inode_by_path(disk, sb, sub_path);

            if (found_inode != -1) {
                current_inode = found_inode;
            } else {
                if (!r) {
                    fclose(disk);
                    throw runtime_error("no existe la ruta padre para el archivo");
                }

                // Crear carpeta intermedia recursivamente
                int new_ino = allocate_inode(disk, sb, '0', 664, user.logged.uid, 1, 0);
                int new_blk = allocate_block(disk, sb);

                Structs::Inodes folder_inode;
                fseek(disk, sb.s_inode_start + (new_ino * sizeof(Structs::Inodes)), SEEK_SET);
                fread(&folder_inode, sizeof(Structs::Inodes), 1, disk);
                folder_inode.i_block[0] = new_blk;
                fseek(disk, sb.s_inode_start + (new_ino * sizeof(Structs::Inodes)), SEEK_SET);
                fwrite(&folder_inode, sizeof(Structs::Inodes), 1, disk);

                Structs::Folderblock fb;
                for (int k = 0; k < 4; k++) {
                    fb.b_content[k].b_inodo = -1;
                    memset(fb.b_content[k].b_name, 0, sizeof(fb.b_content[k].b_name));
                }
                strcpy(fb.b_content[0].b_name, ".");
                fb.b_content[0].b_inodo = new_ino;
                strcpy(fb.b_content[1].b_name, "..");
                fb.b_content[1].b_inodo = current_inode;

                fseek(disk, sb.s_block_start + (new_blk * sizeof(Structs::Folderblock)), SEEK_SET);
                fwrite(&fb, sizeof(Structs::Folderblock), 1, disk);

                add_folder_entry(disk, sb, current_inode, path_parts[i], new_ino);
                current_inode = new_ino;
            }
        }

        // Preparar contenido del archivo
        string data = "";
        if (!cont.empty()) {
            ifstream cont_file(cont);
            if (cont_file.is_open()) {
                stringstream buffer;
                buffer << cont_file.rdbuf();
                data = buffer.str();
                cont_file.close();
            }
            if (has_size && size > (int)data.size()) {
                int diff = size - data.size();
                for (int i = 0; i < diff; i++) {
                    data += to_string(i % 10);
                }
            } else if (has_size && size < (int)data.size()) {
                data = data.substr(0, size);
            }
        } else if (has_size) {
            for (int i = 0; i < size; i++) {
                data += to_string(i % 10);
            }
        }

        // Crear inodo para el archivo
        int file_ino = allocate_inode(disk, sb, '1', 664, user.logged.uid, 1, data.size());

        Structs::Inodes file_inode;
        fseek(disk, sb.s_inode_start + (file_ino * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&file_inode, sizeof(Structs::Inodes), 1, disk);

        int num_blocks = (data.size() + 63) / 64;
        if (num_blocks == 0 && data.empty()) num_blocks = 0;

        for (int b = 0; b < min(num_blocks, 12); b++) {
            int blk = allocate_block(disk, sb);
            file_inode.i_block[b] = blk;
            string chunk = data.substr(b * 64, 64);
            Structs::Fileblock fb;
            memset(fb.b_content, 0, sizeof(fb.b_content));
            memcpy(fb.b_content, chunk.data(), chunk.size());

            fseek(disk, sb.s_block_start + (blk * sizeof(Structs::Folderblock)), SEEK_SET);
            fwrite(&fb, sizeof(Structs::Fileblock), 1, disk);
        }

        if (num_blocks > 12) {
            int ptr_blk = allocate_block(disk, sb);
            file_inode.i_block[12] = ptr_blk;
            Structs::Pointerblock pb;
            for (int k = 0; k < 16; k++) pb.b_pointers[k] = -1;

            for (int b = 12; b < num_blocks && (b - 12) < 16; b++) {
                int blk = allocate_block(disk, sb);
                pb.b_pointers[b - 12] = blk;
                string chunk = data.substr(b * 64, 64);
                Structs::Fileblock fb;
                memset(fb.b_content, 0, sizeof(fb.b_content));
                memcpy(fb.b_content, chunk.data(), chunk.size());

                fseek(disk, sb.s_block_start + (blk * sizeof(Structs::Folderblock)), SEEK_SET);
                fwrite(&fb, sizeof(Structs::Fileblock), 1, disk);
            }

            fseek(disk, sb.s_block_start + (ptr_blk * sizeof(Structs::Folderblock)), SEEK_SET);
            fwrite(&pb, sizeof(Structs::Pointerblock), 1, disk);
        }

        fseek(disk, sb.s_inode_start + (file_ino * sizeof(Structs::Inodes)), SEEK_SET);
        fwrite(&file_inode, sizeof(Structs::Inodes), 1, disk);

        add_folder_entry(disk, sb, current_inode, filename, file_ino);

        // Registrar en Journal si es EXT3
        if (sb.s_filesystem_type == 3) {
            add_journal_entry(disk, partition, sb, "mkfile", path, data);
        }

        fseek(disk, partition.part_start, SEEK_SET);
        fwrite(&sb, sizeof(Structs::Superblock), 1, disk);
        fclose(disk);

        shared.response("MKFILE", "archivo creado correctamente: " + path);
    }
    catch (exception &e) {
        shared.handler("MKFILE", e.what());
    }
}

void FileManager::cat(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        string file_path = "";

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "file1") || shared.compare(id, "file") || shared.compare(id, "filen")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') {
                    current = current.substr(1, current.length() - 2);
                }
                file_path = current;
                break;
            }
        }

        if (file_path.empty()) {
            throw runtime_error("requiere parámetro obligatorio -file1");
        }

        vector<string> path_parts = getpath(file_path);
        if (path_parts.empty()) {
            throw runtime_error("ruta de archivo inválida");
        }

        FILE *disk = fopen(pth.c_str(), "rb");
        if (disk == NULL) {
            throw runtime_error("no se pudo abrir el disco");
        }

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        int inode_idx = find_inode_by_path(disk, sb, path_parts);
        if (inode_idx == -1) {
            fclose(disk);
            throw runtime_error("archivo no encontrado: " + file_path);
        }

        Structs::Inodes inode;
        fseek(disk, sb.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&inode, sizeof(Structs::Inodes), 1, disk);

        if (inode.i_type != '1') {
            fclose(disk);
            throw runtime_error("la ruta especificada corresponde a un directorio, no a un archivo");
        }

        string content = read_file_content(disk, sb, inode_idx);
        fclose(disk);

        cout << "\n--- [CAT: " << file_path << "] ---" << endl;
        cout << content << endl;
        cout << "--- [FIN ARCHIVO] ---\n" << endl;
    }
    catch (exception &e) {
        shared.handler("CAT", e.what());
    }
}

void FileManager::add_journal_entry(FILE *disk, const Structs::Partition &partition, const Structs::Superblock &sb,
                                    const string &operation, const string &path, const string &content) {
    if (sb.s_filesystem_type != 3) {
        return;
    }

    const int JOURNAL_COUNT = 50;
    long journal_start = partition.part_start + sizeof(Structs::Superblock);

    Structs::Journal j_entry;
    int target_idx = -1;
    int next_count = 1;

    for (int i = 0; i < JOURNAL_COUNT; i++) {
        fseek(disk, journal_start + (i * sizeof(Structs::Journal)), SEEK_SET);
        if (fread(&j_entry, sizeof(Structs::Journal), 1, disk) == 1) {
            if (j_entry.j_count > 0 && strlen(j_entry.j_content.i_operation) > 0) {
                next_count = j_entry.j_count + 1;
            } else {
                target_idx = i;
                break;
            }
        } else {
            target_idx = i;
            break;
        }
    }

    if (target_idx == -1) {
        return;
    }

    Structs::Journal new_journal;
    new_journal.j_count = next_count;
    memset(&new_journal.j_content, 0, sizeof(new_journal.j_content));
    strncpy(new_journal.j_content.i_operation, operation.c_str(), sizeof(new_journal.j_content.i_operation) - 1);
    strncpy(new_journal.j_content.i_path, path.c_str(), sizeof(new_journal.j_content.i_path) - 1);
    strncpy(new_journal.j_content.i_content, content.c_str(), sizeof(new_journal.j_content.i_content) - 1);
    new_journal.j_content.i_date = static_cast<float>(time(nullptr));

    fseek(disk, journal_start + (target_idx * sizeof(Structs::Journal)), SEEK_SET);
    fwrite(&new_journal, sizeof(Structs::Journal), 1, disk);
}

void FileManager::record_journal(string disk_path, const Structs::Partition &partition,
                                const string &operation, const string &path, const string &content) {
    FILE *disk = fopen(disk_path.c_str(), "rb+");
    if (disk == NULL) return;

    Structs::Superblock sb;
    fseek(disk, partition.part_start, SEEK_SET);
    if (fread(&sb, sizeof(Structs::Superblock), 1, disk) == 1) {
        if (sb.s_filesystem_type == 3) {
            FileManager fm;
            fm.add_journal_entry(disk, partition, sb, operation, path, content);
        }
    }
    fclose(disk);
}