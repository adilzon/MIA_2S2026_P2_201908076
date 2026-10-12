#include "../lib/filesystem.h"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

using namespace std;

FileSystem::FileSystem(Mount m) {
    mount = m;
}

void FileSystem::mkfs(vector<string> context) {
    vector<string> required = {"id"};
    string id;
    string type = "Full";
    string fs = "2fs";

    for (auto current : context) {
        string id_ = shared.lower(current.substr(0, current.find('=')));
        current.erase(0, id_.length() + 1);
        if (current.front() == '\"') {
            current = current.substr(1, current.length() - 2);
        }

        if (shared.compare(id_, "id")) {
            auto itr = find(required.begin(), required.end(), "id");
            if (itr != required.end()) required.erase(itr);
            id = current;
        } else if (shared.compare(id_, "type")) {
            type = current;
        } else if (shared.compare(id_, "fs")) {
            fs = current;
        }
    }
    if (!required.empty()) {
        shared.handler("MKFS", "requiere parámetro obligatorio -id");
        return;
    }
    mkfs(id, type, fs);
}

void FileSystem::mkfs(string id, string t, string fs) {
    try {
        if (!shared.compare(t, "fast") && !shared.compare(t, "full")) {
            throw runtime_error("-type necesita valores específicos (Fast, Full)");
        } else if (!shared.compare(fs, "2fs") && !shared.compare(fs, "3fs")) {
            throw runtime_error("-fs necesita valores específicos (2fs, 3fs)");
        }

        string p;
        Structs::Partition partition = mount.getmount(id, &p);

        int n = 0;
        if (shared.compare(fs, "2fs")) {
            n = floor((double)(partition.part_size - sizeof(Structs::Superblock)) /
                      (4.0 + sizeof(Structs::Inodes) + 3.0 * sizeof(Structs::Fileblock)));
        } else {
            // EXT3: constante de 50 estructuras de Journaling
            n = floor((double)(partition.part_size - sizeof(Structs::Superblock) - 50.0 * sizeof(Structs::Journal)) /
                      (4.0 + sizeof(Structs::Inodes) + 3.0 * sizeof(Structs::Fileblock)));
        }

        if (n <= 2) {
            throw runtime_error("El tamaño de la partición es demasiado pequeño para el sistema de archivos");
        }

        Structs::Superblock spr;
        spr.s_inodes_count = n;
        spr.s_blocks_count = 3 * n;
        spr.s_free_inodes_count = n - 2; // root + users.txt
        spr.s_free_blocks_count = (3 * n) - 2; // root folder block + users.txt file block
        spr.s_mtime = time(nullptr);
        spr.s_umtime = time(nullptr);
        spr.s_mnt_count = 1;
        spr.s_magic = 0xEF53;
        spr.s_inode_size = sizeof(Structs::Inodes);
        spr.s_block_size = sizeof(Structs::Folderblock);
        spr.s_fist_ino = 2;
        spr.s_first_blo = 2;

        if (shared.compare(fs, "2fs")) {
            spr.s_filesystem_type = 2;
            ext2(spr, partition, n, p);
        } else {
            spr.s_filesystem_type = 3;
            ext3(spr, partition, n, p);
        }

        shared.response("MKFS", "formateo realizado con éxito (-id=" + id + ")");
    }
    catch (exception &e) {
        shared.handler("MKFS", e.what());
    }
}

void FileSystem::ext2(Structs::Superblock spr, Structs::Partition p, int n, string path) {
    spr.s_bm_inode_start = p.part_start + sizeof(Structs::Superblock);
    spr.s_bm_block_start = spr.s_bm_inode_start + n;
    spr.s_inode_start = spr.s_bm_block_start + (3 * n);
    spr.s_block_start = spr.s_inode_start + (n * sizeof(Structs::Inodes));

    FILE *bfile = fopen(path.c_str(), "rb+");
    if (bfile == NULL) {
        throw runtime_error("No se pudo abrir el disco para formatear");
    }

    // Inicializar bitmaps con '0'
    char zero = '0';
    fseek(bfile, spr.s_bm_inode_start, SEEK_SET);
    for (int i = 0; i < n; i++) fwrite(&zero, sizeof(zero), 1, bfile);

    fseek(bfile, spr.s_bm_block_start, SEEK_SET);
    for (int i = 0; i < (3 * n); i++) fwrite(&zero, sizeof(zero), 1, bfile);

    // Inicializar inodos vacíos
    Structs::Inodes empty_inode;
    fseek(bfile, spr.s_inode_start, SEEK_SET);
    for (int i = 0; i < n; i++) fwrite(&empty_inode, sizeof(Structs::Inodes), 1, bfile);

    // Inicializar bloques vacíos
    Structs::Folderblock empty_folder;
    for (int k = 0; k < 4; k++) {
        empty_folder.b_content[k].b_inodo = -1;
        empty_folder.b_content[k].b_name[0] = '\0';
    }
    fseek(bfile, spr.s_block_start, SEEK_SET);
    for (int i = 0; i < (3 * n); i++) fwrite(&empty_folder, sizeof(Structs::Folderblock), 1, bfile);

    // Inodo 0: Raíz "/"
    Structs::Inodes root_inode;
    root_inode.i_uid = 1;
    root_inode.i_gid = 1;
    root_inode.i_size = 0;
    root_inode.i_atime = spr.s_umtime;
    root_inode.i_ctime = spr.s_umtime;
    root_inode.i_mtime = spr.s_umtime;
    root_inode.i_type = '0'; // Carpeta
    root_inode.i_perm = 664;
    root_inode.i_block[0] = 0;

    // Bloque 0: Contenido de Raíz
    Structs::Folderblock root_folder;
    strcpy(root_folder.b_content[0].b_name, ".");
    root_folder.b_content[0].b_inodo = 0;
    strcpy(root_folder.b_content[1].b_name, "..");
    root_folder.b_content[1].b_inodo = 0;
    strcpy(root_folder.b_content[2].b_name, "users.txt");
    root_folder.b_content[2].b_inodo = 1;
    strcpy(root_folder.b_content[3].b_name, "-");
    root_folder.b_content[3].b_inodo = -1;

    // Inodo 1: Archivo "users.txt"
    string data = "1,G,root\n1,U,root,root,123\n";
    Structs::Inodes users_inode;
    users_inode.i_uid = 1;
    users_inode.i_gid = 1;
    users_inode.i_size = data.size();
    users_inode.i_atime = spr.s_umtime;
    users_inode.i_ctime = spr.s_umtime;
    users_inode.i_mtime = spr.s_umtime;
    users_inode.i_type = '1'; // Archivo
    users_inode.i_perm = 664;
    users_inode.i_block[0] = 1;

    // Bloque 1: Contenido de users.txt
    Structs::Fileblock users_fileblock;
    memset(users_fileblock.b_content, 0, sizeof(users_fileblock.b_content));
    strncpy(users_fileblock.b_content, data.c_str(), sizeof(users_fileblock.b_content) - 1);

    // Marcar bitmaps
    char one = '1';
    fseek(bfile, spr.s_bm_inode_start, SEEK_SET);
    fwrite(&one, 1, 1, bfile);
    fwrite(&one, 1, 1, bfile);

    fseek(bfile, spr.s_bm_block_start, SEEK_SET);
    fwrite(&one, 1, 1, bfile);
    fwrite(&one, 1, 1, bfile);

    // Escribir inodos
    fseek(bfile, spr.s_inode_start, SEEK_SET);
    fwrite(&root_inode, sizeof(Structs::Inodes), 1, bfile);
    fwrite(&users_inode, sizeof(Structs::Inodes), 1, bfile);

    // Escribir bloques
    fseek(bfile, spr.s_block_start, SEEK_SET);
    fwrite(&root_folder, sizeof(Structs::Folderblock), 1, bfile);
    fwrite(&users_fileblock, sizeof(Structs::Fileblock), 1, bfile);

    // Guardar Superblock actualizado
    fseek(bfile, p.part_start, SEEK_SET);
    fwrite(&spr, sizeof(Structs::Superblock), 1, bfile);

    fclose(bfile);
}
void FileSystem::ext3(Structs::Superblock spr, Structs::Partition p, int n, string path) {
    spr.s_bm_inode_start = p.part_start + sizeof(Structs::Superblock) + (50 * sizeof(Structs::Journal));
    spr.s_bm_block_start = spr.s_bm_inode_start + n;
    spr.s_inode_start = spr.s_bm_block_start + (3 * n);
    spr.s_block_start = spr.s_inode_start + (n * sizeof(Structs::Inodes));

    FILE *bfile = fopen(path.c_str(), "rb+");
    if (bfile == NULL) {
        throw runtime_error("No se pudo abrir el disco para formatear ext3");
    }

    // Inicializar Journaling (50 entradas fijas)
    Structs::Journal empty_journal;
    empty_journal.j_count = 0;
    memset(&empty_journal.j_content, 0, sizeof(empty_journal.j_content));
    fseek(bfile, p.part_start + sizeof(Structs::Superblock), SEEK_SET);
    for (int i = 0; i < 50; i++) fwrite(&empty_journal, sizeof(Structs::Journal), 1, bfile);

    // Bitmaps con '0'
    char zero = '0';
    fseek(bfile, spr.s_bm_inode_start, SEEK_SET);
    for (int i = 0; i < n; i++) fwrite(&zero, sizeof(zero), 1, bfile);

    fseek(bfile, spr.s_bm_block_start, SEEK_SET);
    for (int i = 0; i < (3 * n); i++) fwrite(&zero, sizeof(zero), 1, bfile);

    // Inodos y bloques vacíos
    Structs::Inodes empty_inode;
    fseek(bfile, spr.s_inode_start, SEEK_SET);
    for (int i = 0; i < n; i++) fwrite(&empty_inode, sizeof(Structs::Inodes), 1, bfile);

    Structs::Folderblock empty_folder;
    for (int k = 0; k < 4; k++) {
        empty_folder.b_content[k].b_inodo = -1;
        empty_folder.b_content[k].b_name[0] = '\0';
    }
    fseek(bfile, spr.s_block_start, SEEK_SET);
    for (int i = 0; i < (3 * n); i++) fwrite(&empty_folder, sizeof(Structs::Folderblock), 1, bfile);

    // Raíz e Inodo users.txt
    Structs::Inodes root_inode;
    root_inode.i_uid = 1; root_inode.i_gid = 1; root_inode.i_size = 0;
    root_inode.i_atime = spr.s_umtime; root_inode.i_ctime = spr.s_umtime; root_inode.i_mtime = spr.s_umtime;
    root_inode.i_type = '0'; root_inode.i_perm = 664; root_inode.i_block[0] = 0;

    Structs::Folderblock root_folder;
    strcpy(root_folder.b_content[0].b_name, "."); root_folder.b_content[0].b_inodo = 0;
    strcpy(root_folder.b_content[1].b_name, ".."); root_folder.b_content[1].b_inodo = 0;
    strcpy(root_folder.b_content[2].b_name, "users.txt"); root_folder.b_content[2].b_inodo = 1;
    strcpy(root_folder.b_content[3].b_name, "-"); root_folder.b_content[3].b_inodo = -1;

    string data = "1,G,root\n1,U,root,root,123\n";
    Structs::Inodes users_inode;
    users_inode.i_uid = 1; users_inode.i_gid = 1; users_inode.i_size = data.size();
    users_inode.i_atime = spr.s_umtime; users_inode.i_ctime = spr.s_umtime; users_inode.i_mtime = spr.s_umtime;
    users_inode.i_type = '1'; users_inode.i_perm = 664; users_inode.i_block[0] = 1;

    Structs::Fileblock users_fileblock;
    memset(users_fileblock.b_content, 0, sizeof(users_fileblock.b_content));
    strncpy(users_fileblock.b_content, data.c_str(), sizeof(users_fileblock.b_content) - 1);

    char one = '1';
    fseek(bfile, spr.s_bm_inode_start, SEEK_SET);
    fwrite(&one, 1, 1, bfile); fwrite(&one, 1, 1, bfile);

    fseek(bfile, spr.s_bm_block_start, SEEK_SET);
    fwrite(&one, 1, 1, bfile); fwrite(&one, 1, 1, bfile);

    fseek(bfile, spr.s_inode_start, SEEK_SET);
    fwrite(&root_inode, sizeof(Structs::Inodes), 1, bfile);
    fwrite(&users_inode, sizeof(Structs::Inodes), 1, bfile);

    fseek(bfile, spr.s_block_start, SEEK_SET);
    fwrite(&root_folder, sizeof(Structs::Folderblock), 1, bfile);
    fwrite(&users_fileblock, sizeof(Structs::Fileblock), 1, bfile);

    fseek(bfile, p.part_start, SEEK_SET);
    fwrite(&spr, sizeof(Structs::Superblock), 1, bfile);

    fclose(bfile);
}