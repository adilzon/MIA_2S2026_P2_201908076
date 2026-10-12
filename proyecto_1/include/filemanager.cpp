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
                    if (folder.b_content[k].b_inodo != -1) {
                        string entry_name = folder.b_content[k].b_name;
                        if (shared.compare(entry_name, part) || 
                            (part.length() > 11 && shared.compare(entry_name, part.substr(0, 11)))) {
                            current_inode = folder.b_content[k].b_inodo;
                            found = true;
                            break;
                        }
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

// =========================================================================
// PERMISOS UGO
// =========================================================================
bool FileManager::check_permission(const Structs::Inodes &inode, const Users::User &logged, int req_perm) {
    if (logged.uid == 1 || shared.compare(logged.user, "root")) {
        return true;
    }

    int p = inode.i_perm;
    int u_perm = (p / 100) % 10;
    int g_perm = (p / 10) % 10;
    int o_perm = p % 10;

    int perm_level = o_perm;
    if (inode.i_uid == logged.uid) {
        perm_level = u_perm;
    } else if (inode.i_gid == logged.gid) {
        perm_level = g_perm;
    }

    // req_perm: 4 = R, 2 = W, 1 = X
    return (perm_level & req_perm) == req_perm;
}

bool FileManager::check_permission_recursive(FILE *disk, Structs::Superblock &sb, int inode_idx, const Users::User &logged, int req_perm) {
    if (logged.uid == 1 || shared.compare(logged.user, "root")) return true;

    Structs::Inodes in;
    fseek(disk, sb.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    if (fread(&in, sizeof(Structs::Inodes), 1, disk) != 1) return false;

    if (!check_permission(in, logged, req_perm)) return false;

    if (in.i_type == '0') {
        for (int b = 0; b < 12; b++) {
            if (in.i_block[b] != -1) {
                Structs::Folderblock fb;
                fseek(disk, sb.s_block_start + (in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                if (fread(&fb, sizeof(Structs::Folderblock), 1, disk) == 1) {
                    for (int k = 0; k < 4; k++) {
                        if (fb.b_content[k].b_inodo != -1) {
                            string name = fb.b_content[k].b_name;
                            if (name != "." && name != "..") {
                                if (!check_permission_recursive(disk, sb, fb.b_content[k].b_inodo, logged, req_perm)) {
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
}

void FileManager::free_inode_and_blocks(FILE *disk, Structs::Superblock &sb, int inode_idx) {
    Structs::Inodes in;
    fseek(disk, sb.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    if (fread(&in, sizeof(Structs::Inodes), 1, disk) != 1) return;

    if (in.i_type == '0') {
        for (int b = 0; b < 12; b++) {
            if (in.i_block[b] != -1) {
                Structs::Folderblock fb;
                fseek(disk, sb.s_block_start + (in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                if (fread(&fb, sizeof(Structs::Folderblock), 1, disk) == 1) {
                    for (int k = 0; k < 4; k++) {
                        if (fb.b_content[k].b_inodo != -1) {
                            string name = fb.b_content[k].b_name;
                            if (name != "." && name != "..") {
                                free_inode_and_blocks(disk, sb, fb.b_content[k].b_inodo);
                            }
                        }
                    }
                }
                fseek(disk, sb.s_bm_block_start + in.i_block[b], SEEK_SET);
                char zero = '0';
                fwrite(&zero, 1, 1, disk);
                sb.s_free_blocks_count++;
            }
        }
    } else {
        for (int b = 0; b < 12; b++) {
            if (in.i_block[b] != -1) {
                fseek(disk, sb.s_bm_block_start + in.i_block[b], SEEK_SET);
                char zero = '0';
                fwrite(&zero, 1, 1, disk);
                sb.s_free_blocks_count++;
            }
        }
        if (in.i_block[12] != -1) {
            Structs::Pointerblock pb;
            fseek(disk, sb.s_block_start + (in.i_block[12] * sizeof(Structs::Folderblock)), SEEK_SET);
            if (fread(&pb, sizeof(Structs::Pointerblock), 1, disk) == 1) {
                for (int p = 0; p < 16; p++) {
                    if (pb.b_pointers[p] != -1) {
                        fseek(disk, sb.s_bm_block_start + pb.b_pointers[p], SEEK_SET);
                        char zero = '0';
                        fwrite(&zero, 1, 1, disk);
                        sb.s_free_blocks_count++;
                    }
                }
            }
            fseek(disk, sb.s_bm_block_start + in.i_block[12], SEEK_SET);
            char zero = '0';
            fwrite(&zero, 1, 1, disk);
            sb.s_free_blocks_count++;
        }
    }

    fseek(disk, sb.s_bm_inode_start + inode_idx, SEEK_SET);
    char zero = '0';
    fwrite(&zero, 1, 1, disk);
    sb.s_free_inodes_count++;

    Structs::Inodes empty_in;
    fseek(disk, sb.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    fwrite(&empty_in, sizeof(Structs::Inodes), 1, disk);
}

bool FileManager::remove_folder_entry(FILE *disk, Structs::Superblock &sb, int parent_inode_idx, string name) {
    Structs::Inodes parent_in;
    fseek(disk, sb.s_inode_start + (parent_inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    if (fread(&parent_in, sizeof(Structs::Inodes), 1, disk) != 1) return false;

    for (int b = 0; b < 12; b++) {
        if (parent_in.i_block[b] != -1) {
            Structs::Folderblock fb;
            fseek(disk, sb.s_block_start + (parent_in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
            if (fread(&fb, sizeof(Structs::Folderblock), 1, disk) == 1) {
                for (int k = 0; k < 4; k++) {
                    if (fb.b_content[k].b_inodo != -1 && 
                        (shared.compare(fb.b_content[k].b_name, name) || 
                         (name.length() > 11 && shared.compare(fb.b_content[k].b_name, name.substr(0, 11))))) {
                        fb.b_content[k].b_inodo = -1;
                        memset(fb.b_content[k].b_name, 0, sizeof(fb.b_content[k].b_name));
                        fseek(disk, sb.s_block_start + (parent_in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                        fwrite(&fb, sizeof(Structs::Folderblock), 1, disk);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// =========================================================================
// 4. REMOVE -path=...
// =========================================================================
void FileManager::remove_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        string path = "";
        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "path")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                path = current;
            }
        }

        if (path.empty()) throw runtime_error("requiere parámetro obligatorio -path");

        vector<string> path_parts = getpath(path);
        if (path_parts.empty()) throw runtime_error("no se puede eliminar la raíz '/'");

        string target_name = path_parts.back();
        vector<string> parent_parts(path_parts.begin(), path_parts.end() - 1);

        FILE *disk = fopen(pth.c_str(), "rb+");
        if (disk == NULL) throw runtime_error("no se pudo abrir el disco");

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        int parent_inode = find_inode_by_path(disk, sb, parent_parts);
        if (parent_inode == -1) {
            fclose(disk);
            throw runtime_error("directorio padre no encontrado");
        }

        int target_inode = find_inode_by_path(disk, sb, path_parts);
        if (target_inode == -1) {
            fclose(disk);
            throw runtime_error("archivo o carpeta no encontrado: " + path);
        }

        // Validar permisos de escritura (2) recursivamente
        if (!check_permission_recursive(disk, sb, target_inode, user.logged, 2)) {
            fclose(disk);
            throw runtime_error("permiso denegado: no tiene permisos para eliminar uno o más elementos");
        }

        // Eliminar inodos y bloques recursivamente
        free_inode_and_blocks(disk, sb, target_inode);
        remove_folder_entry(disk, sb, parent_inode, target_name);

        if (sb.s_filesystem_type == 3) {
            add_journal_entry(disk, partition, sb, "remove", path, "-");
        }

        fseek(disk, partition.part_start, SEEK_SET);
        fwrite(&sb, sizeof(Structs::Superblock), 1, disk);
        fclose(disk);

        shared.response("REMOVE", "eliminado correctamente: " + path);
    }
    catch (exception &e) {
        shared.handler("REMOVE", e.what());
    }
}

// =========================================================================
// 5. RENAME -path=... -name=...
// =========================================================================
void FileManager::rename_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        string path = "";
        string new_name = "";

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "path")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                path = current;
            } else if (shared.compare(id, "name")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                new_name = current;
            }
        }

        if (path.empty() || new_name.empty()) {
            throw runtime_error("requiere parámetros obligatorios (-path, -name)");
        }

        vector<string> path_parts = getpath(path);
        if (path_parts.empty()) throw runtime_error("no se puede renombrar la raíz '/'");

        string old_name = path_parts.back();
        vector<string> parent_parts(path_parts.begin(), path_parts.end() - 1);

        FILE *disk = fopen(pth.c_str(), "rb+");
        if (disk == NULL) throw runtime_error("no se pudo abrir el disco");

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        int parent_inode = find_inode_by_path(disk, sb, parent_parts);
        if (parent_inode == -1) {
            fclose(disk);
            throw runtime_error("directorio padre no encontrado");
        }

        int target_inode = find_inode_by_path(disk, sb, path_parts);
        if (target_inode == -1) {
            fclose(disk);
            throw runtime_error("archivo o carpeta no encontrado: " + path);
        }

        // Verificar que no exista ya otro archivo/carpeta con el nuevo nombre
        vector<string> new_full_parts = parent_parts;
        new_full_parts.push_back(new_name);
        if (find_inode_by_path(disk, sb, new_full_parts) != -1) {
            fclose(disk);
            throw runtime_error("ya existe un elemento con el nombre: " + new_name);
        }

        Structs::Inodes in;
        fseek(disk, sb.s_inode_start + (target_inode * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&in, sizeof(Structs::Inodes), 1, disk);

        if (!check_permission(in, user.logged, 2)) {
            fclose(disk);
            throw runtime_error("permiso denegado: no tiene permiso de escritura para renombrar");
        }

        // Actualizar nombre en bloque del directorio padre
        Structs::Inodes parent_in;
        fseek(disk, sb.s_inode_start + (parent_inode * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&parent_in, sizeof(Structs::Inodes), 1, disk);

        bool renamed = false;
        for (int b = 0; b < 12; b++) {
            if (parent_in.i_block[b] != -1) {
                Structs::Folderblock fb;
                fseek(disk, sb.s_block_start + (parent_in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                fread(&fb, sizeof(Structs::Folderblock), 1, disk);
                for (int k = 0; k < 4; k++) {
                    if (fb.b_content[k].b_inodo == target_inode) {
                        memset(fb.b_content[k].b_name, 0, sizeof(fb.b_content[k].b_name));
                        strncpy(fb.b_content[k].b_name, new_name.c_str(), sizeof(fb.b_content[k].b_name) - 1);
                        fseek(disk, sb.s_block_start + (parent_in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                        fwrite(&fb, sizeof(Structs::Folderblock), 1, disk);
                        renamed = true;
                        break;
                    }
                }
                if (renamed) break;
            }
        }

        if (!renamed) {
            fclose(disk);
            throw runtime_error("error al actualizar el nombre en el directorio");
        }

        if (sb.s_filesystem_type == 3) {
            add_journal_entry(disk, partition, sb, "rename", path, new_name);
        }

        fseek(disk, partition.part_start, SEEK_SET);
        fwrite(&sb, sizeof(Structs::Superblock), 1, disk);
        fclose(disk);

        shared.response("RENAME", old_name + " renombrado a " + new_name);
    }
    catch (exception &e) {
        shared.handler("RENAME", e.what());
    }
}

// =========================================================================
// 6. COPY -path=... -destino=...
// =========================================================================
int FileManager::copy_inode_recursive(FILE *disk_src, Structs::Superblock &sb_src, int src_inode_idx,
                                      FILE *disk_dst, Structs::Superblock &sb_dst, int uid, int gid) {
    Structs::Inodes src_in;
    fseek(disk_src, sb_src.s_inode_start + (src_inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    fread(&src_in, sizeof(Structs::Inodes), 1, disk_src);

    int new_inode_idx = allocate_inode(disk_dst, sb_dst, src_in.i_type, src_in.i_perm, uid, gid, src_in.i_size);

    if (src_in.i_type == '0') {
        int new_blk = allocate_block(disk_dst, sb_dst);

        Structs::Inodes new_in;
        fseek(disk_dst, sb_dst.s_inode_start + (new_inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&new_in, sizeof(Structs::Inodes), 1, disk_dst);
        new_in.i_block[0] = new_blk;
        fseek(disk_dst, sb_dst.s_inode_start + (new_inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
        fwrite(&new_in, sizeof(Structs::Inodes), 1, disk_dst);

        Structs::Folderblock root_fb;
        for (int k = 0; k < 4; k++) {
            root_fb.b_content[k].b_inodo = -1;
            memset(root_fb.b_content[k].b_name, 0, sizeof(root_fb.b_content[k].b_name));
        }
        strcpy(root_fb.b_content[0].b_name, ".");
        root_fb.b_content[0].b_inodo = new_inode_idx;
        strcpy(root_fb.b_content[1].b_name, "..");
        root_fb.b_content[1].b_inodo = new_inode_idx;

        fseek(disk_dst, sb_dst.s_block_start + (new_blk * sizeof(Structs::Folderblock)), SEEK_SET);
        fwrite(&root_fb, sizeof(Structs::Folderblock), 1, disk_dst);

        // Copiar hijos
        for (int b = 0; b < 12; b++) {
            if (src_in.i_block[b] != -1) {
                Structs::Folderblock src_fb;
                fseek(disk_src, sb_src.s_block_start + (src_in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                fread(&src_fb, sizeof(Structs::Folderblock), 1, disk_src);
                for (int k = 0; k < 4; k++) {
                    if (src_fb.b_content[k].b_inodo != -1) {
                        string name = src_fb.b_content[k].b_name;
                        if (name != "." && name != "..") {
                            int child_copy_idx = copy_inode_recursive(disk_src, sb_src, src_fb.b_content[k].b_inodo,
                                                                      disk_dst, sb_dst, uid, gid);
                            add_folder_entry(disk_dst, sb_dst, new_inode_idx, name, child_copy_idx);
                        }
                    }
                }
            }
        }
    } else {
        // Archivo: leer y copiar contenido
        string data = read_file_content(disk_src, sb_src, src_inode_idx);
        Structs::Inodes new_in;
        fseek(disk_dst, sb_dst.s_inode_start + (new_inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&new_in, sizeof(Structs::Inodes), 1, disk_dst);
        new_in.i_size = data.size();

        int num_blocks = (data.size() + 63) / 64;
        for (int b = 0; b < min(num_blocks, 12); b++) {
            int blk = allocate_block(disk_dst, sb_dst);
            new_in.i_block[b] = blk;
            string chunk = data.substr(b * 64, 64);
            Structs::Fileblock fb;
            memset(fb.b_content, 0, sizeof(fb.b_content));
            memcpy(fb.b_content, chunk.data(), chunk.size());
            fseek(disk_dst, sb_dst.s_block_start + (blk * sizeof(Structs::Folderblock)), SEEK_SET);
            fwrite(&fb, sizeof(Structs::Fileblock), 1, disk_dst);
        }
        fseek(disk_dst, sb_dst.s_inode_start + (new_inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
        fwrite(&new_in, sizeof(Structs::Inodes), 1, disk_dst);
    }
    return new_inode_idx;
}

void FileManager::copy_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        string path = "";
        string destino = "";

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "path")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                path = current;
            } else if (shared.compare(id, "destino") || shared.compare(id, "dest")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                destino = current;
            }
        }

        if (path.empty() || destino.empty()) throw runtime_error("requiere parámetros obligatorios (-path, -destino)");

        vector<string> src_parts = getpath(path);
        if (src_parts.empty()) throw runtime_error("ruta de origen no válida");
        string item_name = src_parts.back();

        vector<string> dst_parts = getpath(destino);

        FILE *disk = fopen(pth.c_str(), "rb+");
        if (disk == NULL) throw runtime_error("no se pudo abrir el disco");

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        int src_inode = find_inode_by_path(disk, sb, src_parts);
        if (src_inode == -1) {
            fclose(disk);
            throw runtime_error("elemento de origen no encontrado: " + path);
        }

        int dst_inode = find_inode_by_path(disk, sb, dst_parts);
        if (dst_inode == -1) {
            fclose(disk);
            throw runtime_error("carpeta de destino no encontrada: " + destino);
        }

        // Validar permisos: lectura (4) en origen, escritura (2) en destino
        if (!check_permission_recursive(disk, sb, src_inode, user.logged, 4)) {
            fclose(disk);
            throw runtime_error("permiso denegado: no tiene permiso de lectura en el origen");
        }

        Structs::Inodes dst_in;
        fseek(disk, sb.s_inode_start + (dst_inode * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&dst_in, sizeof(Structs::Inodes), 1, disk);
        if (!check_permission(dst_in, user.logged, 2)) {
            fclose(disk);
            throw runtime_error("permiso denegado: no tiene permiso de escritura en el destino");
        }

        int copied_inode = copy_inode_recursive(disk, sb, src_inode, disk, sb, user.logged.uid, 1);
        add_folder_entry(disk, sb, dst_inode, item_name, copied_inode);

        if (sb.s_filesystem_type == 3) {
            add_journal_entry(disk, partition, sb, "copy", path, destino);
        }

        fseek(disk, partition.part_start, SEEK_SET);
        fwrite(&sb, sizeof(Structs::Superblock), 1, disk);
        fclose(disk);

        shared.response("COPY", "copiado correctamente: " + path + " -> " + destino);
    }
    catch (exception &e) {
        shared.handler("COPY", e.what());
    }
}

// =========================================================================
// 7. MOVE -path=... -destino=...
// =========================================================================
void FileManager::move_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        string path = "";
        string destino = "";

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "path")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                path = current;
            } else if (shared.compare(id, "destino") || shared.compare(id, "dest")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                destino = current;
            }
        }

        if (path.empty() || destino.empty()) throw runtime_error("requiere parámetros obligatorios (-path, -destino)");

        vector<string> src_parts = getpath(path);
        if (src_parts.empty()) throw runtime_error("no se puede mover la raíz '/'");
        string item_name = src_parts.back();
        vector<string> parent_parts(src_parts.begin(), src_parts.end() - 1);

        vector<string> dst_parts = getpath(destino);

        FILE *disk = fopen(pth.c_str(), "rb+");
        if (disk == NULL) throw runtime_error("no se pudo abrir el disco");

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        int src_inode = find_inode_by_path(disk, sb, src_parts);
        if (src_inode == -1) {
            fclose(disk);
            throw runtime_error("elemento de origen no encontrado: " + path);
        }

        int parent_inode = find_inode_by_path(disk, sb, parent_parts);
        if (parent_inode == -1) {
            fclose(disk);
            throw runtime_error("directorio padre de origen no encontrado");
        }

        int dst_inode = find_inode_by_path(disk, sb, dst_parts);
        if (dst_inode == -1) {
            fclose(disk);
            throw runtime_error("directorio de destino no encontrado: " + destino);
        }

        // Permiso de escritura en origen y destino
        Structs::Inodes src_in;
        fseek(disk, sb.s_inode_start + (src_inode * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&src_in, sizeof(Structs::Inodes), 1, disk);
        if (!check_permission(src_in, user.logged, 2)) {
            fclose(disk);
            throw runtime_error("permiso denegado: no tiene permiso de escritura en el origen");
        }

        Structs::Inodes dst_in;
        fseek(disk, sb.s_inode_start + (dst_inode * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&dst_in, sizeof(Structs::Inodes), 1, disk);
        if (!check_permission(dst_in, user.logged, 2)) {
            fclose(disk);
            throw runtime_error("permiso denegado: no tiene permiso de escritura en el destino");
        }

        // Quitar de la carpeta padre actual y agregar al destino (misma partición)
        remove_folder_entry(disk, sb, parent_inode, item_name);
        add_folder_entry(disk, sb, dst_inode, item_name, src_inode);

        if (sb.s_filesystem_type == 3) {
            add_journal_entry(disk, partition, sb, "move", path, destino);
        }

        fseek(disk, partition.part_start, SEEK_SET);
        fwrite(&sb, sizeof(Structs::Superblock), 1, disk);
        fclose(disk);

        shared.response("MOVE", "movido correctamente: " + path + " -> " + destino);
    }
    catch (exception &e) {
        shared.handler("MOVE", e.what());
    }
}

// =========================================================================
// 8. FIND -path=... -name=...
// =========================================================================
bool FileManager::match_pattern(const string &pattern, const string &text) {
    if (pattern.empty()) return text.empty();
    if (pattern == "*") return true;

    int p_len = pattern.length();
    int t_len = text.length();
    vector<vector<bool>> dp(p_len + 1, vector<bool>(t_len + 1, false));

    dp[0][0] = true;
    for (int i = 1; i <= p_len; i++) {
        if (pattern[i - 1] == '*') dp[i][0] = dp[i - 1][0];
    }

    for (int i = 1; i <= p_len; i++) {
        for (int j = 1; j <= t_len; j++) {
            if (pattern[i - 1] == '?' || pattern[i - 1] == text[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else if (pattern[i - 1] == '*') {
                dp[i][j] = dp[i - 1][j] || dp[i][j - 1];
            }
        }
    }
    return dp[p_len][t_len];
}

void FileManager::search_tree(FILE *disk, Structs::Superblock &sb, int inode_idx, const string &current_path,
                              const string &pattern, vector<string> &results) {
    Structs::Inodes in;
    fseek(disk, sb.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    if (fread(&in, sizeof(Structs::Inodes), 1, disk) != 1) return;

    if (in.i_type == '0') {
        for (int b = 0; b < 12; b++) {
            if (in.i_block[b] != -1) {
                Structs::Folderblock fb;
                fseek(disk, sb.s_block_start + (in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                if (fread(&fb, sizeof(Structs::Folderblock), 1, disk) == 1) {
                    for (int k = 0; k < 4; k++) {
                        if (fb.b_content[k].b_inodo != -1) {
                            string name = fb.b_content[k].b_name;
                            if (name != "." && name != "..") {
                                string full_sub = (current_path == "/") ? ("/" + name) : (current_path + "/" + name);
                                bool matched = match_pattern(pattern, name);
                                if (!matched && name.length() == 11 && pattern.find('.') != string::npos) {
                                    size_t dot_p = pattern.rfind('.');
                                    string ext = pattern.substr(dot_p);
                                    matched = match_pattern(pattern, name + ext.substr(1));
                                }
                                if (matched) {
                                    results.push_back(full_sub);
                                }
                                search_tree(disk, sb, fb.b_content[k].b_inodo, full_sub, pattern, results);
                            }
                        }
                    }
                }
            }
        }
    }
}

void FileManager::find_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        string path = "/";
        string name = "*";

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "path")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                path = current;
            } else if (shared.compare(id, "name")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                name = current;
            }
        }

        vector<string> path_parts = getpath(path);
        FILE *disk = fopen(pth.c_str(), "rb");
        if (disk == NULL) throw runtime_error("no se pudo abrir el disco");

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        int root_inode = find_inode_by_path(disk, sb, path_parts);
        if (root_inode == -1) {
            fclose(disk);
            throw runtime_error("ruta inicial no encontrada: " + path);
        }

        vector<string> results;
        search_tree(disk, sb, root_inode, path, name, results);
        fclose(disk);

        cout << "\n======================================== RESULTADOS DE FIND (" << name << ") ========================================" << endl;
        if (results.empty()) {
            cout << "(No se encontraron coincidencias)" << endl;
        } else {
            for (const string &res : results) {
                cout << "  > " << res << endl;
            }
        }
        cout << string(95, '=') << "\n" << endl;
    }
    catch (exception &e) {
        shared.handler("FIND", e.what());
    }
}

// =========================================================================
// 9. CHOWN -path=... -usuario=... [-r]
// =========================================================================
void FileManager::chown_recursive(FILE *disk, Structs::Superblock &sb, int inode_idx, int new_uid) {
    Structs::Inodes in;
    fseek(disk, sb.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    if (fread(&in, sizeof(Structs::Inodes), 1, disk) != 1) return;

    in.i_uid = new_uid;
    fseek(disk, sb.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
    fwrite(&in, sizeof(Structs::Inodes), 1, disk);

    if (in.i_type == '0') {
        for (int b = 0; b < 12; b++) {
            if (in.i_block[b] != -1) {
                Structs::Folderblock fb;
                fseek(disk, sb.s_block_start + (in.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                if (fread(&fb, sizeof(Structs::Folderblock), 1, disk) == 1) {
                    for (int k = 0; k < 4; k++) {
                        if (fb.b_content[k].b_inodo != -1) {
                            string name = fb.b_content[k].b_name;
                            if (name != "." && name != "..") {
                                chown_recursive(disk, sb, fb.b_content[k].b_inodo, new_uid);
                            }
                        }
                    }
                }
            }
        }
    }
}

void FileManager::chown_cmd(vector<string> context, Structs::Partition partition, string pth, Users &user) {
    try {
        string path = "";
        string usuario = "";
        bool r = false;

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(id, "path")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                path = current;
            } else if (shared.compare(id, "usuario") || shared.compare(id, "usr") || shared.compare(id, "user")) {
                current.erase(0, id.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                usuario = current;
            } else if (shared.compare(id, "r")) {
                r = true;
            }
        }

        if (path.empty() || usuario.empty()) {
            throw runtime_error("requiere parámetros obligatorios (-path, -usuario)");
        }

        // Obtener UID del nuevo usuario desde users.txt
        string users_txt = user.read_users_file(pth, partition);
        vector<string> lines = user.getElements(users_txt, '\n');
        int target_uid = -1;
        for (const string &line : lines) {
            if (line.empty()) continue;
            vector<string> fields = user.getElements(line, ',');
            if (fields.size() >= 5 && (fields[1] == "U" || fields[1] == "u")) {
                if (shared.compare(fields[3], usuario)) {
                    target_uid = stoi(fields[0]);
                    break;
                }
            }
        }

        if (target_uid == -1) {
            throw runtime_error("usuario no encontrado: " + usuario);
        }

        vector<string> path_parts = getpath(path);
        FILE *disk = fopen(pth.c_str(), "rb+");
        if (disk == NULL) throw runtime_error("no se pudo abrir el disco");

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        fread(&sb, sizeof(Structs::Superblock), 1, disk);

        int target_inode = find_inode_by_path(disk, sb, path_parts);
        if (target_inode == -1) {
            fclose(disk);
            throw runtime_error("archivo o carpeta no encontrado: " + path);
        }

        Structs::Inodes in;
        fseek(disk, sb.s_inode_start + (target_inode * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&in, sizeof(Structs::Inodes), 1, disk);

        // Validaciones: root puede cambiar cualquier archivo; otros usuarios solo los suyos
        if (!shared.compare(user.logged.user, "root") && in.i_uid != user.logged.uid) {
            fclose(disk);
            throw runtime_error("solo root o el propietario pueden cambiar el dueño de este archivo");
        }

        if (r) {
            chown_recursive(disk, sb, target_inode, target_uid);
        } else {
            in.i_uid = target_uid;
            fseek(disk, sb.s_inode_start + (target_inode * sizeof(Structs::Inodes)), SEEK_SET);
            fwrite(&in, sizeof(Structs::Inodes), 1, disk);
        }

        if (sb.s_filesystem_type == 3) {
            add_journal_entry(disk, partition, sb, "chown", path, usuario);
        }

        fseek(disk, partition.part_start, SEEK_SET);
        fwrite(&sb, sizeof(Structs::Superblock), 1, disk);
        fclose(disk);

        shared.response("CHOWN", "propietario actualizado a " + usuario + " para: " + path);
    }
    catch (exception &e) {
        shared.handler("CHOWN", e.what());
    }
}

// =========================================================================
// 10. LOSS -id=...
// =========================================================================
void FileManager::loss_cmd(vector<string> context, Mount &mount_inst) {
    try {
        string id = "";
        for (auto current : context) {
            string key = shared.lower(current.substr(0, current.find('=')));
            if (shared.compare(key, "id")) {
                current.erase(0, key.length() + 1);
                if (current.front() == '\"') current = current.substr(1, current.length() - 2);
                id = current;
            }
        }

        if (id.empty()) throw runtime_error("requiere parámetro obligatorio -id");

        string disk_path;
        Structs::Partition partition = mount_inst.getmount(id, &disk_path);
        if (partition.part_start == -1) throw runtime_error("partición no montada: " + id);

        FILE *disk = fopen(disk_path.c_str(), "rb+");
        if (disk == NULL) throw runtime_error("no se pudo abrir el disco");

        Structs::Superblock sb;
        fseek(disk, partition.part_start, SEEK_SET);
        if (fread(&sb, sizeof(Structs::Superblock), 1, disk) != 1) {
            fclose(disk);
            throw runtime_error("error al leer el superbloque");
        }

        if (sb.s_filesystem_type != 3) {
            fclose(disk);
            throw runtime_error("el comando loss solo aplica a sistemas EXT3");
        }

        // Mostrar estado previo
        cout << "\n[LOSS] Iniciando simulación de pérdida en partición " << id << "..." << endl;
        cout << "[LOSS] Bloques ocupados previos: " << (sb.s_blocks_count - sb.s_free_blocks_count) << endl;
        cout << "[LOSS] Inodos ocupados previos: " << (sb.s_inodes_count - sb.s_free_inodes_count) << endl;

        // Limpiar bitmaps con '0' y áreas de inodos y bloques con '\0'
        int bm_inodes_size = sb.s_inodes_count;
        int bm_blocks_size = sb.s_blocks_count;
        int inodes_table_size = sb.s_inodes_count * sizeof(Structs::Inodes);
        int blocks_table_size = sb.s_blocks_count * sizeof(Structs::Folderblock);

        // 1. Limpiar Bitmap de Inodos
        fseek(disk, sb.s_bm_inode_start, SEEK_SET);
        vector<char> zero_char(bm_inodes_size, '0');
        fwrite(zero_char.data(), 1, bm_inodes_size, disk);

        // 2. Limpiar Bitmap de Bloques
        fseek(disk, sb.s_bm_block_start, SEEK_SET);
        vector<char> zero_blocks(bm_blocks_size, '0');
        fwrite(zero_blocks.data(), 1, bm_blocks_size, disk);

        // 3. Limpiar Área de Inodos
        fseek(disk, sb.s_inode_start, SEEK_SET);
        vector<char> zero_null(65536, '\0');
        int remaining = inodes_table_size;
        while (remaining > 0) {
            int to_w = min(remaining, (int)zero_null.size());
            fwrite(zero_null.data(), 1, to_w, disk);
            remaining -= to_w;
        }

        // 4. Limpiar Área de Bloques
        fseek(disk, sb.s_block_start, SEEK_SET);
        remaining = blocks_table_size;
        while (remaining > 0) {
            int to_w = min(remaining, (int)zero_null.size());
            fwrite(zero_null.data(), 1, to_w, disk);
            remaining -= to_w;
        }

        fclose(disk);
        shared.response("LOSS", "pérdida del sistema de archivos simulada exitosamente (-id=" + id + ")");
        cout << "[LOSS] El Journaling permanece intacto para recuperación futura.\n" << endl;
    }
    catch (exception &e) {
        shared.handler("LOSS", e.what());
    }
}