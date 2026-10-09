#include "../lib/report.h"
#include "../lib/disco.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <filesystem>

using namespace std;

Report::Report() {}

static string clean_quotes(string s) {
    while (!s.empty() && (s.front() == '\"' || s.front() == '\'')) {
        s.erase(0, 1);
    }
    while (!s.empty() && (s.back() == '\"' || s.back() == '\'')) {
        s.pop_back();
    }
    return s;
}

void Report::execute_dot(string dot_content, string output_path) {
    try {
        filesystem::path op(output_path);
        if (op.has_parent_path()) {
            filesystem::create_directories(op.parent_path());
        }

        string ext = op.extension().string();
        if (!ext.empty() && ext.front() == '.') ext.erase(0, 1);
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == "jpg" || ext == "jpeg") ext = "jpg";
        else if (ext != "png" && ext != "pdf" && ext != "svg") ext = "jpg";

        string dot_file = output_path + ".dot";
        ofstream out(dot_file);
        if (!out.is_open()) {
            throw runtime_error("no se pudo crear archivo temporal dot: " + dot_file);
        }
        out << dot_content;
        out.close();

        string cmd = "dot -T" + ext + " \"" + dot_file + "\" -o \"" + output_path + "\"";
        int res = system(cmd.c_str());
        remove(dot_file.c_str());

        if (res == 0) {
            shared.response("REPORT", "reporte generado correctamente: " + output_path);
        } else {
            shared.handler("REPORT", "error al ejecutar graphviz dot");
        }
    } catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::generar(vector<string> context, Mount &m) {
    mount = m;
    string name = "";
    string path = "";
    string id = "";
    string path_file_ls = "";

    for (string current : context) {
        string id_ = shared.lower(current.substr(0, current.find('=')));
        current.erase(0, id_.length() + 1);
        current = clean_quotes(current);

        if (shared.compare(id_, "name")) {
            name = current;
        } else if (shared.compare(id_, "id")) {
            id = current;
        } else if (shared.compare(id_, "path")) {
            path = current;
        } else if (shared.compare(id_, "path_file_ls")) {
            path_file_ls = current;
        }
    }

    if (name.empty() || path.empty() || id.empty()) {
        shared.handler("REPORT", "faltan parámetros obligatorios (-name, -path, -id)");
        return;
    }

    if (shared.compare(name, "MBR")) {
        mbr(path, id);
    } else if (shared.compare(name, "DISK") || shared.compare(name, "DKS")) {
        disk_rep(path, id);
    } else if (shared.compare(name, "INODE")) {
        inode(path, id);
    } else if (shared.compare(name, "BLOCK")) {
        block(path, id);
    } else if (shared.compare(name, "BM_INODE") || shared.compare(name, "BMINODE")) {
        bminode(path, id);
    } else if (shared.compare(name, "BM_BLOCK") || shared.compare(name, "BMBLOCK")) {
        bmblock(path, id);
    } else if (shared.compare(name, "SB")) {
        sb(path, id);
    } else if (shared.compare(name, "TREE")) {
        tree(path, id);
    } else if (shared.compare(name, "FILE")) {
        file_rep(path, id, path_file_ls);
    } else if (shared.compare(name, "LS")) {
        ls_rep(path, id, path_file_ls);
    } else if (shared.compare(name, "JOURNALING")) {
        journaling(path, id);
    } else {
        shared.handler("REPORT", "tipo de reporte no reconocido: " + name);
    }
}

void Report::mbr(string p, string id) {
    try {
        string disk_path;
        mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::MBR disco;
        fread(&disco, sizeof(Structs::MBR), 1, file);

        struct tm *tm = localtime(&disco.mbr_fecha_creacion);
        char mostrar_fecha[30];
        strftime(mostrar_fecha, 30, "%Y/%m/%d %H:%M:%S", tm);

        stringstream ss;
        ss << "digraph G {\n"
           << "  rankdir=TB;\n"
           << "  node [shape=plaintext fontname=\"Arial\"];\n"
           << "  general [label=<\n"
           << "    <table border='0' cellborder='1' cellspacing='0'>\n"
           << "      <tr><td colspan='2' bgcolor='#1B4F72'><font color='white'><b>REPORTE MBR</b></font></td></tr>\n"
           << "      <tr><td bgcolor='#D4E6F1'><b>NOMBRE</b></td><td bgcolor='#D4E6F1'><b>VALOR</b></td></tr>\n"
           << "      <tr><td>mbr_tamano</td><td>" << disco.mbr_tamano << "</td></tr>\n"
           << "      <tr><td>mbr_fecha_creacion</td><td>" << mostrar_fecha << "</td></tr>\n"
           << "      <tr><td>mbr_disk_signature</td><td>" << disco.mbr_disk_signature << "</td></tr>\n"
           << "      <tr><td>disk_fit</td><td>" << disco.disk_fit << "</td></tr>\n";

        Structs::Partition partitions[4] = {
            disco.mbr_Partition_1, disco.mbr_Partition_2, disco.mbr_Partition_3, disco.mbr_Partition_4
        };

        for (int i = 0; i < 4; i++) {
            if (partitions[i].part_status == '1') {
                ss << "      <tr><td colspan='2' bgcolor='#2E86C1'><font color='white'><b>PARTICION " << (i + 1) << "</b></font></td></tr>\n"
                   << "      <tr><td>part_status</td><td>" << partitions[i].part_status << "</td></tr>\n"
                   << "      <tr><td>part_type</td><td>" << partitions[i].part_type << "</td></tr>\n"
                   << "      <tr><td>part_fit</td><td>" << partitions[i].part_fit << "</td></tr>\n"
                   << "      <tr><td>part_start</td><td>" << partitions[i].part_start << "</td></tr>\n"
                   << "      <tr><td>part_size</td><td>" << partitions[i].part_size << "</td></tr>\n"
                   << "      <tr><td>part_name</td><td>" << partitions[i].part_name << "</td></tr>\n";

                if (partitions[i].part_type == 'E' || partitions[i].part_type == 'e') {
                    vector<Structs::EBR> ebrs = disk.getlogics(partitions[i], disk_path);
                    int ebr_count = 1;
                    for (const auto &ebr : ebrs) {
                        ss << "      <tr><td colspan='2' bgcolor='#5DADE2'><font color='white'><b>EBR " << ebr_count++ << "</b></font></td></tr>\n"
                           << "      <tr><td>part_status</td><td>" << ebr.part_status << "</td></tr>\n"
                           << "      <tr><td>part_fit</td><td>" << ebr.part_fit << "</td></tr>\n"
                           << "      <tr><td>part_start</td><td>" << ebr.part_start << "</td></tr>\n"
                           << "      <tr><td>part_size</td><td>" << ebr.part_size << "</td></tr>\n"
                           << "      <tr><td>part_next</td><td>" << ebr.part_next << "</td></tr>\n"
                           << "      <tr><td>part_name</td><td>" << ebr.part_name << "</td></tr>\n";
                    }
                }
            }
        }

        ss << "    </table>\n  >];\n}\n";
        fclose(file);

        execute_dot(ss.str(), p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::disk_rep(string p, string id) {
    try {
        string disk_path;
        mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::MBR disco;
        fread(&disco, sizeof(Structs::MBR), 1, file);

        double total = disco.mbr_tamano;
        Structs::Partition parts[4] = {
            disco.mbr_Partition_1, disco.mbr_Partition_2, disco.mbr_Partition_3, disco.mbr_Partition_4
        };

        struct Chunk {
            int start;
            int size;
            string label;
            bool is_ext = false;
            Structs::Partition part;
        };
        vector<Chunk> chunks;

        for (int i = 0; i < 4; i++) {
            if (parts[i].part_status == '1') {
                Chunk ch;
                ch.start = parts[i].part_start;
                ch.size = parts[i].part_size;
                ch.part = parts[i];
                if (parts[i].part_type == 'E' || parts[i].part_type == 'e') {
                    ch.is_ext = true;
                    ch.label = "Extendida";
                } else {
                    ch.label = "Primaria: " + string(parts[i].part_name);
                }
                chunks.push_back(ch);
            }
        }
        sort(chunks.begin(), chunks.end(), [](const Chunk &a, const Chunk &b) {
            return a.start < b.start;
        });

        stringstream ss;
        ss << "digraph G {\n"
           << "  rankdir=TB;\n"
           << "  node [shape=plaintext fontname=\"Arial\"];\n"
           << "  disk [label=<\n"
           << "    <table border='0' cellborder='1' cellspacing='0'>\n"
           << "      <tr>\n"
           << "        <td rowspan='2' bgcolor='#D5D8DC'>MBR</td>\n";

        int curr = sizeof(Structs::MBR);
        for (const auto &ch : chunks) {
            if (ch.start > curr) {
                int free_sz = ch.start - curr;
                double pct = (free_sz / total) * 100.0;
                ss << "        <td rowspan='2' bgcolor='#EDBB99'>Libre<br/>" << fixed << setprecision(2) << pct << "%</td>\n";
            }
            double pct = (ch.size / total) * 100.0;
            if (ch.is_ext) {
                vector<Structs::EBR> ebrs = disk.getlogics(ch.part, disk_path);
                int subcols = max(1, (int)ebrs.size() * 2);
                ss << "        <td colspan='" << subcols << "' bgcolor='#A9CCE3'>Extendida<br/>"
                   << fixed << setprecision(2) << pct << "%</td>\n";
            } else {
                ss << "        <td rowspan='2' bgcolor='#A2D9CE'>" << ch.label << "<br/>"
                   << fixed << setprecision(2) << pct << "%</td>\n";
            }
            curr = ch.start + ch.size;
        }
        if (curr < disco.mbr_tamano) {
            int free_sz = disco.mbr_tamano - curr;
            double pct = (free_sz / total) * 100.0;
            ss << "        <td rowspan='2' bgcolor='#EDBB99'>Libre<br/>" << fixed << setprecision(2) << pct << "%</td>\n";
        }
        ss << "      </tr>\n";

        // Segunda fila para lógicas dentro de extendidas
        bool has_ext = false;
        for (const auto &ch : chunks) {
            if (ch.is_ext) {
                has_ext = true;
                break;
            }
        }
        if (has_ext) {
            ss << "      <tr>\n";
            for (const auto &ch : chunks) {
                if (ch.is_ext) {
                    vector<Structs::EBR> ebrs = disk.getlogics(ch.part, disk_path);
                    if (ebrs.empty()) {
                        ss << "        <td bgcolor='#E8F8F5'>Libre</td>\n";
                    } else {
                        for (const auto &ebr : ebrs) {
                            ss << "        <td bgcolor='#D4EFDF'>EBR</td>\n";
                            double lpct = (ebr.part_size / total) * 100.0;
                            ss << "        <td bgcolor='#D5F5E3'>Lógica: " << ebr.part_name << "<br/>"
                               << fixed << setprecision(2) << lpct << "%</td>\n";
                        }
                    }
                }
            }
            ss << "      </tr>\n";
        }

        ss << "    </table>\n  >];\n}\n";
        fclose(file);

        execute_dot(ss.str(), p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::sb(string p, string id) {
    try {
        string disk_path;
        Structs::Partition partition = mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::Superblock spr;
        fseek(file, partition.part_start, SEEK_SET);
        fread(&spr, sizeof(Structs::Superblock), 1, file);
        fclose(file);

        struct tm *tm_m = localtime(&spr.s_mtime);
        char f_mtime[30];
        strftime(f_mtime, 30, "%Y/%m/%d %H:%M:%S", tm_m);

        struct tm *tm_u = localtime(&spr.s_umtime);
        char f_umtime[30];
        strftime(f_umtime, 30, "%Y/%m/%d %H:%M:%S", tm_u);

        stringstream ss;
        ss << "digraph G {\n"
           << "  rankdir=TB;\n"
           << "  node [shape=plaintext fontname=\"Arial\"];\n"
           << "  general [label=<\n"
           << "    <table border='0' cellborder='1' cellspacing='0'>\n"
           << "      <tr><td colspan='2' bgcolor='#196F3D'><font color='white'><b>REPORTE SUPERBLOQUE</b></font></td></tr>\n"
           << "      <tr><td bgcolor='#D5F5E3'><b>NOMBRE</b></td><td bgcolor='#D5F5E3'><b>VALOR</b></td></tr>\n"
           << "      <tr><td>s_filesystem_type</td><td>" << spr.s_filesystem_type << "</td></tr>\n"
           << "      <tr><td>s_inodes_count</td><td>" << spr.s_inodes_count << "</td></tr>\n"
           << "      <tr><td>s_blocks_count</td><td>" << spr.s_blocks_count << "</td></tr>\n"
           << "      <tr><td>s_free_blocks_count</td><td>" << spr.s_free_blocks_count << "</td></tr>\n"
           << "      <tr><td>s_free_inodes_count</td><td>" << spr.s_free_inodes_count << "</td></tr>\n"
           << "      <tr><td>s_mtime</td><td>" << f_mtime << "</td></tr>\n"
           << "      <tr><td>s_umtime</td><td>" << f_umtime << "</td></tr>\n"
           << "      <tr><td>s_mnt_count</td><td>" << spr.s_mnt_count << "</td></tr>\n"
           << "      <tr><td>s_magic</td><td>0xEF53</td></tr>\n"
           << "      <tr><td>s_inode_size</td><td>" << spr.s_inode_size << "</td></tr>\n"
           << "      <tr><td>s_block_size</td><td>" << spr.s_block_size << "</td></tr>\n"
           << "      <tr><td>s_first_ino</td><td>" << spr.s_fist_ino << "</td></tr>\n"
           << "      <tr><td>s_first_blo</td><td>" << spr.s_first_blo << "</td></tr>\n"
           << "      <tr><td>s_bm_inode_start</td><td>" << spr.s_bm_inode_start << "</td></tr>\n"
           << "      <tr><td>s_bm_block_start</td><td>" << spr.s_bm_block_start << "</td></tr>\n"
           << "      <tr><td>s_inode_start</td><td>" << spr.s_inode_start << "</td></tr>\n"
           << "      <tr><td>s_block_start</td><td>" << spr.s_block_start << "</td></tr>\n"
           << "    </table>\n  >];\n}\n";

        execute_dot(ss.str(), p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::inode(string p, string id) {
    try {
        string disk_path;
        Structs::Partition partition = mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::Superblock spr;
        fseek(file, partition.part_start, SEEK_SET);
        fread(&spr, sizeof(Structs::Superblock), 1, file);

        stringstream ss;
        ss << "digraph G {\n"
           << "  rankdir=LR;\n"
           << "  node [shape=plaintext fontname=\"Arial\"];\n";

        char ch;
        int prev_idx = -1;
        for (int i = 0; i < spr.s_inodes_count; i++) {
            fseek(file, spr.s_bm_inode_start + i, SEEK_SET);
            fread(&ch, 1, 1, file);
            if (ch == '1') {
                Structs::Inodes in;
                fseek(file, spr.s_inode_start + (i * sizeof(Structs::Inodes)), SEEK_SET);
                fread(&in, sizeof(Structs::Inodes), 1, file);

                struct tm *tm_a = localtime(&in.i_atime);
                char fa[30]; strftime(fa, 30, "%Y/%m/%d %H:%M", tm_a);

                ss << "  inode" << i << " [label=<\n"
                   << "    <table border='0' cellborder='1' cellspacing='0'>\n"
                   << "      <tr><td colspan='2' bgcolor='#2874A6'><font color='white'><b>INODO " << i << "</b></font></td></tr>\n"
                   << "      <tr><td>i_uid</td><td>" << in.i_uid << "</td></tr>\n"
                   << "      <tr><td>i_gid</td><td>" << in.i_gid << "</td></tr>\n"
                   << "      <tr><td>i_size</td><td>" << in.i_size << "</td></tr>\n"
                   << "      <tr><td>i_atime</td><td>" << fa << "</td></tr>\n";
                string type_str = (in.i_type == '0' || in.i_type == 0) ? "0" : "1";
                ss << "      <tr><td>i_type</td><td>" << type_str << "</td></tr>\n"
                   << "      <tr><td>i_perm</td><td>" << in.i_perm << "</td></tr>\n";
                for (int b = 0; b < 15; b++) {
                    ss << "      <tr><td>i_block[" << b << "]</td><td>" << in.i_block[b] << "</td></tr>\n";
                }
                ss << "    </table>\n  >];\n";

                if (prev_idx != -1) {
                    ss << "  inode" << prev_idx << " -> inode" << i << " [color=\"#2874A6\"];\n";
                }
                prev_idx = i;
            }
        }
        ss << "}\n";
        fclose(file);

        execute_dot(ss.str(), p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::block(string p, string id) {
    try {
        string disk_path;
        Structs::Partition partition = mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::Superblock spr;
        fseek(file, partition.part_start, SEEK_SET);
        fread(&spr, sizeof(Structs::Superblock), 1, file);

        stringstream ss;
        ss << "digraph G {\n"
           << "  rankdir=LR;\n"
           << "  node [shape=plaintext fontname=\"Arial\"];\n";

        char ch;
        int prev_blk = -1;
        for (int b = 0; b < spr.s_blocks_count; b++) {
            fseek(file, spr.s_bm_block_start + b, SEEK_SET);
            fread(&ch, 1, 1, file);
            if (ch == '1') {
                // Leer como bloque genérico (64 bytes)
                Structs::Folderblock fb;
                fseek(file, spr.s_block_start + (b * sizeof(Structs::Folderblock)), SEEK_SET);
                fread(&fb, sizeof(Structs::Folderblock), 1, file);

                // Determinar si parece carpeta o archivo
                bool is_folder = false;
                for (int k = 0; k < 4; k++) {
                    if (fb.b_content[k].b_inodo != -1 && (strcmp(fb.b_content[k].b_name, ".") == 0 || strcmp(fb.b_content[k].b_name, "..") == 0)) {
                        is_folder = true;
                        break;
                    }
                }

                if (is_folder) {
                    ss << "  block" << b << " [label=<\n"
                       << "    <table border='0' cellborder='1' cellspacing='0'>\n"
                       << "      <tr><td colspan='2' bgcolor='#1E8449'><font color='white'><b>BLOQUE CARPETA " << b << "</b></font></td></tr>\n"
                       << "      <tr><td bgcolor='#A9DFBF'><b>NOMBRE</b></td><td bgcolor='#A9DFBF'><b>INODO</b></td></tr>\n";
                    for (int k = 0; k < 4; k++) {
                        ss << "      <tr><td>" << fb.b_content[k].b_name << "</td><td>" << fb.b_content[k].b_inodo << "</td></tr>\n";
                    }
                    ss << "    </table>\n  >];\n";
                } else {
                    Structs::Fileblock flb;
                    memcpy(&flb, &fb, sizeof(Structs::Fileblock));
                    string safe_content = "";
                    for (int c = 0; c < 64 && flb.b_content[c] != '\0'; c++) {
                        if (flb.b_content[c] == '<') safe_content += "&lt;";
                        else if (flb.b_content[c] == '>') safe_content += "&gt;";
                        else if (flb.b_content[c] == '&') safe_content += "&amp;";
                        else if (flb.b_content[c] == '\n') safe_content += "<br/>";
                        else safe_content += flb.b_content[c];
                    }
                    ss << "  block" << b << " [label=<\n"
                       << "    <table border='0' cellborder='1' cellspacing='0'>\n"
                       << "      <tr><td bgcolor='#B7950B'><font color='white'><b>BLOQUE ARCHIVO " << b << "</b></font></td></tr>\n"
                       << "      <tr><td>" << (safe_content.empty() ? "(vacio)" : safe_content) << "</td></tr>\n"
                       << "    </table>\n  >];\n";
                }

                if (prev_blk != -1) {
                    ss << "  block" << prev_blk << " -> block" << b << " [color=\"#1E8449\"];\n";
                }
                prev_blk = b;
            }
        }

        ss << "}\n";
        fclose(file);

        execute_dot(ss.str(), p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::bminode(string p, string id) {
    try {
        string disk_path;
        Structs::Partition partition = mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::Superblock spr;
        fseek(file, partition.part_start, SEEK_SET);
        fread(&spr, sizeof(Structs::Superblock), 1, file);

        filesystem::path op(p);
        if (op.has_parent_path()) {
            filesystem::create_directories(op.parent_path());
        }

        ofstream outfile(p);
        if (!outfile.is_open()) {
            fclose(file);
            throw runtime_error("no se pudo crear archivo de reporte bitmap");
        }

        fseek(file, spr.s_bm_inode_start, SEEK_SET);
        char ch;
        int count = 0;
        for (int i = 0; i < spr.s_inodes_count; i++) {
            fread(&ch, 1, 1, file);
            outfile << ch;
            count++;
            if (count == 20) {
                outfile << "\n";
                count = 0;
            }
        }
        if (count != 0) outfile << "\n";

        outfile.close();
        fclose(file);
        shared.response("REPORT", "reporte bm_inode generado correctamente: " + p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::bmblock(string p, string id) {
    try {
        string disk_path;
        Structs::Partition partition = mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::Superblock spr;
        fseek(file, partition.part_start, SEEK_SET);
        fread(&spr, sizeof(Structs::Superblock), 1, file);

        filesystem::path op(p);
        if (op.has_parent_path()) {
            filesystem::create_directories(op.parent_path());
        }

        ofstream outfile(p);
        if (!outfile.is_open()) {
            fclose(file);
            throw runtime_error("no se pudo crear archivo de reporte bitmap");
        }

        fseek(file, spr.s_bm_block_start, SEEK_SET);
        char ch;
        int count = 0;
        for (int i = 0; i < spr.s_blocks_count; i++) {
            fread(&ch, 1, 1, file);
            outfile << ch;
            count++;
            if (count == 20) {
                outfile << "\n";
                count = 0;
            }
        }
        if (count != 0) outfile << "\n";

        outfile.close();
        fclose(file);
        shared.response("REPORT", "reporte bm_block generado correctamente: " + p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::file_rep(string p, string id, string file_path) {
    try {
        if (file_path.empty()) {
            throw runtime_error("el reporte file requiere parámetro -path_file_ls");
        }
        string disk_path;
        Structs::Partition partition = mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::Superblock spr;
        fseek(file, partition.part_start, SEEK_SET);
        fread(&spr, sizeof(Structs::Superblock), 1, file);

        vector<string> parts = fileManager.getpath(file_path);
        int inode_idx = fileManager.find_inode_by_path(file, spr, parts);
        if (inode_idx == -1) {
            fclose(file);
            throw runtime_error("archivo no encontrado: " + file_path);
        }

        Structs::Inodes in;
        fseek(file, spr.s_inode_start + (inode_idx * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&in, sizeof(Structs::Inodes), 1, file);

        string content = fileManager.read_file_content(file, spr, inode_idx);
        fclose(file);

        string safe_content = "";
        for (char c : content) {
            if (c == '<') safe_content += "&lt;";
            else if (c == '>') safe_content += "&gt;";
            else if (c == '&') safe_content += "&amp;";
            else if (c == '\n') safe_content += "<br/>";
            else safe_content += c;
        }

        stringstream ss;
        ss << "digraph G {\n"
           << "  rankdir=TB;\n"
           << "  node [shape=plaintext fontname=\"Arial\"];\n"
           << "  filerep [label=<\n"
           << "    <table border='0' cellborder='1' cellspacing='0'>\n"
           << "      <tr><td colspan='2' bgcolor='#2E4053'><font color='white'><b>REPORTE FILE: " << file_path << "</b></font></td></tr>\n"
           << "      <tr><td><b>Inodo</b></td><td>" << inode_idx << "</td></tr>\n"
           << "      <tr><td><b>Tamaño</b></td><td>" << in.i_size << " bytes</td></tr>\n"
           << "      <tr><td><b>UID / GID</b></td><td>" << in.i_uid << " / " << in.i_gid << "</td></tr>\n"
           << "      <tr><td colspan='2' bgcolor='#D5D8DC'><b>CONTENIDO</b></td></tr>\n"
           << "      <tr><td colspan='2' align='left'>" << (safe_content.empty() ? "(vacio)" : safe_content) << "</td></tr>\n"
           << "    </table>\n  >];\n}\n";

        execute_dot(ss.str(), p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::ls_rep(string p, string id, string dir_path) {
    try {
        if (dir_path.empty()) {
            dir_path = "/";
        }
        string disk_path;
        Structs::Partition partition = mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::Superblock spr;
        fseek(file, partition.part_start, SEEK_SET);
        fread(&spr, sizeof(Structs::Superblock), 1, file);

        vector<string> parts = fileManager.getpath(dir_path);
        int dir_inode = fileManager.find_inode_by_path(file, spr, parts);
        if (dir_inode == -1) {
            fclose(file);
            throw runtime_error("directorio no encontrado: " + dir_path);
        }

        Structs::Inodes din;
        fseek(file, spr.s_inode_start + (dir_inode * sizeof(Structs::Inodes)), SEEK_SET);
        fread(&din, sizeof(Structs::Inodes), 1, file);

        stringstream ss;
        ss << "digraph G {\n"
           << "  rankdir=TB;\n"
           << "  node [shape=plaintext fontname=\"Arial\"];\n"
           << "  lsrep [label=<\n"
           << "    <table border='0' cellborder='1' cellspacing='0'>\n"
           << "      <tr><td colspan='7' bgcolor='#117864'><font color='white'><b>REPORTE LS: " << dir_path << "</b></font></td></tr>\n"
           << "      <tr bgcolor='#A3E4D7'>\n"
           << "        <td><b>Permisos</b></td><td><b>Owner</b></td><td><b>Grupo</b></td>"
           << "        <td><b>Size (bytes)</b></td><td><b>Fecha</b></td><td><b>Tipo</b></td><td><b>Nombre</b></td>\n"
           << "      </tr>\n";

        for (int b = 0; b < 12; b++) {
            if (din.i_block[b] != -1) {
                Structs::Folderblock fb;
                fseek(file, spr.s_block_start + (din.i_block[b] * sizeof(Structs::Folderblock)), SEEK_SET);
                fread(&fb, sizeof(Structs::Folderblock), 1, file);

                for (int k = 0; k < 4; k++) {
                    if (fb.b_content[k].b_inodo != -1) {
                        int child_ino = fb.b_content[k].b_inodo;
                        Structs::Inodes cin;
                        fseek(file, spr.s_inode_start + (child_ino * sizeof(Structs::Inodes)), SEEK_SET);
                        fread(&cin, sizeof(Structs::Inodes), 1, file);

                        struct tm *tm_c = localtime(&cin.i_mtime);
                        char fc[30]; strftime(fc, 30, "%Y/%m/%d %H:%M", tm_c);

                        string tipo = (cin.i_type == '0') ? "Carpeta" : "Archivo";
                        ss << "      <tr>\n"
                           << "        <td>" << cin.i_perm << "</td>\n"
                           << "        <td>" << cin.i_uid << "</td>\n"
                           << "        <td>" << cin.i_gid << "</td>\n"
                           << "        <td>" << cin.i_size << "</td>\n"
                           << "        <td>" << fc << "</td>\n"
                           << "        <td>" << tipo << "</td>\n"
                           << "        <td>" << fb.b_content[k].b_name << "</td>\n"
                           << "      </tr>\n";
                    }
                }
            }
        }

        ss << "    </table>\n  >];\n}\n";
        fclose(file);

        execute_dot(ss.str(), p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::tree(string p, string id) {
    try {
        string disk_path;
        Structs::Partition partition = mount.getmount(id, &disk_path);

        FILE *file = fopen(disk_path.c_str(), "rb");
        if (file == NULL) throw runtime_error("disco no existente");

        Structs::Superblock spr;
        fseek(file, partition.part_start, SEEK_SET);
        fread(&spr, sizeof(Structs::Superblock), 1, file);

        stringstream ss;
        ss << "digraph G {\n"
           << "  rankdir=LR;\n"
           << "  node [shape=plaintext fontname=\"Arial\"];\n";

        char ch;
        for (int i = 0; i < spr.s_inodes_count; i++) {
            fseek(file, spr.s_bm_inode_start + i, SEEK_SET);
            fread(&ch, 1, 1, file);
            if (ch == '1') {
                Structs::Inodes in;
                fseek(file, spr.s_inode_start + (i * sizeof(Structs::Inodes)), SEEK_SET);
                fread(&in, sizeof(Structs::Inodes), 1, file);

                ss << "  inode" << i << " [label=<\n"
                   << "    <table border='0' cellborder='1' cellspacing='0'>\n"
                   << "      <tr><td colspan='2' bgcolor='#1B4F72'><font color='white'><b>INODO " << i << "</b></font></td></tr>\n"
                   << "      <tr><td>i_uid</td><td>" << in.i_uid << "</td></tr>\n"
                   << "      <tr><td>i_gid</td><td>" << in.i_gid << "</td></tr>\n"
                   << "      <tr><td>i_size</td><td>" << in.i_size << "</td></tr>\n"
                   << "      <tr><td>i_type</td><td>" << in.i_type << "</td></tr>\n"
                   << "      <tr><td>i_perm</td><td>" << in.i_perm << "</td></tr>\n";
                for (int b = 0; b < 15; b++) {
                    ss << "      <tr><td>i_block[" << b << "]</td><td port='p" << b << "'>" << in.i_block[b] << "</td></tr>\n";
                }
                ss << "    </table>\n  >];\n";

                // Enlaces a bloques
                for (int b = 0; b < 12; b++) {
                    if (in.i_block[b] != -1) {
                        int blk = in.i_block[b];
                        ss << "  inode" << i << ":p" << b << " -> block" << blk << ";\n";

                        if (in.i_type == '0') {
                            Structs::Folderblock fb;
                            fseek(file, spr.s_block_start + (blk * sizeof(Structs::Folderblock)), SEEK_SET);
                            fread(&fb, sizeof(Structs::Folderblock), 1, file);

                            ss << "  block" << blk << " [label=<\n"
                               << "    <table border='0' cellborder='1' cellspacing='0'>\n"
                               << "      <tr><td colspan='2' bgcolor='#196F3D'><font color='white'><b>BLOQUE CARPETA " << blk << "</b></font></td></tr>\n"
                               << "      <tr><td bgcolor='#A9DFBF'><b>NOMBRE</b></td><td bgcolor='#A9DFBF'><b>INODO</b></td></tr>\n";
                            for (int k = 0; k < 4; k++) {
                                ss << "      <tr><td>" << fb.b_content[k].b_name << "</td><td port='p" << k << "'>" << fb.b_content[k].b_inodo << "</td></tr>\n";
                            }
                            ss << "    </table>\n  >];\n";

                            // Enlaces de carpeta a inodos hijos
                            for (int k = 0; k < 4; k++) {
                                if (fb.b_content[k].b_inodo != -1) {
                                    string nm = fb.b_content[k].b_name;
                                    if (nm != "." && nm != "..") {
                                        ss << "  block" << blk << ":p" << k << " -> inode" << fb.b_content[k].b_inodo << ";\n";
                                    }
                                }
                            }
                        } else {
                            Structs::Fileblock flb;
                            fseek(file, spr.s_block_start + (blk * sizeof(Structs::Folderblock)), SEEK_SET);
                            fread(&flb, sizeof(Structs::Fileblock), 1, file);

                            string safe_content = "";
                            for (int c = 0; c < 64 && flb.b_content[c] != '\0'; c++) {
                                if (flb.b_content[c] == '<') safe_content += "&lt;";
                                else if (flb.b_content[c] == '>') safe_content += "&gt;";
                                else if (flb.b_content[c] == '&') safe_content += "&amp;";
                                else if (flb.b_content[c] == '\n') safe_content += "<br/>";
                                else safe_content += flb.b_content[c];
                            }
                            ss << "  block" << blk << " [label=<\n"
                               << "    <table border='0' cellborder='1' cellspacing='0'>\n"
                               << "      <tr><td bgcolor='#B7950B'><font color='white'><b>BLOQUE ARCHIVO " << blk << "</b></font></td></tr>\n"
                               << "      <tr><td>" << (safe_content.empty() ? "(vacio)" : safe_content) << "</td></tr>\n"
                               << "    </table>\n  >];\n";
                        }
                    }
                }
            }
        }

        ss << "}\n";
        fclose(file);

        execute_dot(ss.str(), p);
    }
    catch (exception &e) {
        shared.handler("REPORT", e.what());
    }
}

void Report::journaling(string p, string id) {
    shared.response("REPORT", "Reporte journaling procesado");
}
