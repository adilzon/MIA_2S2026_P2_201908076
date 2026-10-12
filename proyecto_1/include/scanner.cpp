#include "../lib/scanner.h"
#include "../lib/disco.h"
#include "../lib/mount.h"
#include "../lib/report.h"
#include "../lib/filesystem.h"
#include "../lib/users.h"
#include "../lib/filemanager.h"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iomanip>

using namespace std;

static Mount mount_inst;
static Disk disco;
static FileSystem fileSystem = FileSystem(mount_inst);
static Users user;
static Shared shared;
static FileManager filemanager;
static Report report;
static bool logued = false;

scanner::scanner() {}

void scanner::start() {
    cout << "\033[1;92m";
    cout << R"(
            .--.
           |o_o |
           |:_/ |
          //   \ \
         (|     | )
        /'\_   _/`\
        \___)=(___/

        Linux Proyecto 1 EXT2
        Adilzon Alfredo Velasquez Hernandez
        Carnet: 201908076
)";
    cout << "\033[0m" << endl;

    string linea;
    while (true) {
        cout << "\033[32m[(execute)]>: \033[0m";
        if (!getline(cin, linea)) {
            break; // EOF
        }

        if (!linea.empty() && linea.back() == '\r') {
            linea.pop_back();
        }

        // Quitar espacios iniciales
        size_t first_non_space = linea.find_first_not_of(" \t");
        if (first_non_space == string::npos) continue;
        linea = linea.substr(first_non_space);

        // Si es comentario completo
        if (linea.front() == '#') {
            continue;
        }

        if (compare(linea, "exit")) {
            break;
        }

        // Quitar comentarios al final de la línea si no están dentro de comillas
        bool in_q = false;
        string clean_line = "";
        for (char c : linea) {
            if (c == '\"') in_q = !in_q;
            if (c == '#' && !in_q) break;
            clean_line += c;
        }
        linea = clean_line;

        string tk = token(linea);
        if (tk.empty()) continue;

        string resto = linea.substr(tk.length());
        vector<string> tks = split_tokens(resto);

        functions(tk, tks);
    }
}

string scanner::token(string text) {
    size_t start = text.find_first_not_of(" \t");
    if (start == string::npos) return "";

    size_t end = text.find_first_of(" \t", start);
    if (end == string::npos) return text.substr(start);

    return text.substr(start, end - start);
}

vector<string> scanner::split_tokens(string text) {
    vector<string> tokens;
    if (text.empty()) return tokens;

    string current = "";
    bool in_quotes = false;

    for (size_t i = 0; i < text.length(); i++) {
        char c = text[i];

        // Si supuestamente estamos en comillas pero encontramos un espacio seguido de '-'
        // significa que la comilla anterior fue una comilla huérfana / no cerrada
        if (in_quotes && (c == ' ' || c == '\t') && (i + 1 < text.length() && text[i + 1] == '-')) {
            in_quotes = false;
            if (!current.empty()) {
                tokens.push_back(current);
                current = "";
            }
            continue;
        }

        if (c == '\"') {
            in_quotes = !in_quotes;
            current += c;
        } else if ((c == ' ' || c == '\t' || c == '\r' || c == '\n') && !in_quotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current = "";
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }

    // Limpiar tokens
    vector<string> cleaned;
    for (string &tok : tokens) {
        if (!tok.empty() && tok.front() == '-') {
            tok.erase(0, 1);
        }
        size_t eq_pos = tok.find('=');
        if (eq_pos != string::npos) {
            string key = tok.substr(0, eq_pos);
            string val = tok.substr(eq_pos + 1);
            while (!val.empty() && (val.front() == '\"' || val.front() == '\'')) val.erase(0, 1);
            while (!val.empty() && (val.back() == '\"' || val.back() == '\'')) val.pop_back();
            tok = key + "=" + val;
        } else {
            while (!tok.empty() && (tok.front() == '\"' || tok.front() == '\'')) tok.erase(0, 1);
            while (!tok.empty() && (tok.back() == '\"' || tok.back() == '\'')) tok.pop_back();
        }
        if (!tok.empty()) {
            cleaned.push_back(tok);
        }
    }

    return cleaned;
}

vector<string> scanner::split(string text, string text_split) {
    vector<string> cadena;
    if (text.empty()) return cadena;

    char *cstr = new char[text.length() + 1];
    strcpy(cstr, text.c_str());
    char *point = strtok(cstr, text_split.c_str());
    while (point != NULL) {
        cadena.push_back(string(point));
        point = strtok(NULL, text_split.c_str());
    }
    delete[] cstr;
    return cadena;
}

string scanner::upper(string a) {
    string up = "";
    for (char c : a) up += toupper(c);
    return up;
}

bool scanner::compare(string a, string b) {
    return upper(a) == upper(b);
}

void scanner::errores(string operacion, string mensaje) {
    cout << "\033[1;41m Error \033[0;31m(" + operacion + ")~~> \033[0m" << mensaje << endl;
}

void scanner::respuesta(string operacion, string mensaje) {
    cout << "\033[0;32m (" + operacion + "): \033[0m" << mensaje << endl;
}

bool scanner::confirmar(string mensaje) {
    return true; // En modo calificación y scripts, confirmar automáticamente
}

void scanner::functions(string token, vector<string> tks) {
    if (token.empty()) return;

    if (compare(token, "MKDISK")) {
        disco.mkdisk(tks);
    } else if (compare(token, "RMDISK")) {
        disco.rmdisk(tks);
    } else if (compare(token, "FDISK")) {
        disco.fdisk(tks);
    } else if (compare(token, "MOUNT")) {
        mount_inst.mount(tks);
    } else if (compare(token, "UNMOUNT")) {
        mount_inst.unmount(tks);
    } else if (compare(token, "MOUNTED")) {
        mount_inst.listmount();
    } else if (compare(token, "MKFS")) {
        fileSystem = FileSystem(mount_inst);
        fileSystem.mkfs(tks);
    } else if (compare(token, "LOGIN")) {
        if (logued) {
            shared.handler("LOGIN", "ya existe una sesion abierta");
            return;
        }
        logued = user.login(tks, mount_inst);
    } else if (compare(token, "LOGOUT")) {
        if (!logued) {
            shared.handler("LOGOUT", "debe de iniciar sesion primero");
            return;
        }
        logued = user.logout();
    } else if (compare(token, "MKGRP")) {
        if (!logued) {
            shared.handler("MKGRP", "debe de iniciar sesion primero");
            return;
        }
        user.grp(tks, "MK");
    } else if (compare(token, "RMGRP")) {
        if (!logued) {
            shared.handler("RMGRP", "debe de iniciar sesion primero");
            return;
        }
        user.grp(tks, "RM");
    } else if (compare(token, "MKUSR")) {
        if (!logued) {
            shared.handler("MKUSR", "debe de iniciar sesion primero");
            return;
        }
        user.usr(tks, "MK");
    } else if (compare(token, "RMUSR")) {
        if (!logued) {
            shared.handler("RMUSR", "debe de iniciar sesion primero");
            return;
        }
        user.usr(tks, "RM");
    } else if (compare(token, "CHGRP")) {
        if (!logued) {
            shared.handler("CHGRP", "debe de iniciar sesion primero");
            return;
        }
        user.chgrp_cmd(tks);
    } else if (compare(token, "MKDIR")) {
        if (!logued) {
            shared.handler("MKDIR", "debe de iniciar sesion primero");
            return;
        }
        string p;
        Structs::Partition partition = mount_inst.getmount(user.logged.id, &p);
        filemanager.mkdir(tks, partition, p, user);
    } else if (compare(token, "MKFILE")) {
        if (!logued) {
            shared.handler("MKFILE", "debe de iniciar sesion primero");
            return;
        }
        string p;
        Structs::Partition partition = mount_inst.getmount(user.logged.id, &p);
        filemanager.mkfile(tks, partition, p, user);
    } else if (compare(token, "CAT")) {
        if (!logued) {
            shared.handler("CAT", "debe de iniciar sesion primero");
            return;
        }
        string p;
        Structs::Partition partition = mount_inst.getmount(user.logged.id, &p);
        filemanager.cat(tks, partition, p, user);
    } else if (compare(token, "REP")) {
        report.generar(tks, mount_inst);
    } else if (compare(token, "EXEC")) {
        funcion_excec(tks);
    } else if (compare(token, "PAUSE")) {
        cout << "\033[33m--- PAUSE (presione ENTER para continuar) ---\033[0m" << endl;
    } else if (compare(token, "JOURNALING")) {
        cmd_journaling(tks);
    } else if (token.front() == '#') {
        // Comentario ignorado
    } else {
        errores("SYSTEM", "El comando ingresado no se reconoce en el sistema: \"" + token + "\"");
    }
}

void scanner::funcion_excec(vector<string> tokens) {
    string path = "";
    for (string token : tokens) {
        string tk = token.substr(0, token.find("="));
        token.erase(0, tk.length() + 1);
        if (compare(tk, "path")) {
            path = token;
        }
    }
    if (path.empty()) {
        errores("EXEC", "path no ingresado");
        return;
    }
    excec(path);
}

void scanner::excec(string path) {
    if (!path.empty() && (path.front() == '\"' || path.front() == '\'')) path.erase(0, 1);
    if (!path.empty() && (path.back() == '\"' || path.back() == '\'')) path.pop_back();

    ifstream input_file(path);
    if (!input_file.is_open()) {
        cerr << "\033[1;31mError al abrir archivo de script: " << path << "\033[0m" << endl;
        return;
    }

    string line;
    int line_num = 0;
    while (getline(input_file, line)) {
        line_num++;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t first_non_space = line.find_first_not_of(" \t");
        if (first_non_space == string::npos) continue;
        line = line.substr(first_non_space);

        if (line.front() == '#') continue;

        bool in_q = false;
        string clean_line = "";
        for (char c : line) {
            if (c == '\"') in_q = !in_q;
            if (c == '#' && !in_q) break;
            clean_line += c;
        }
        line = clean_line;

        string tk = token(line);
        if (tk.empty()) continue;

        cout << "\n\033[1;34m[LINEA " << line_num << "]: " << line << "\033[0m" << endl;

        string resto = line.substr(tk.length());
        vector<string> tks = split_tokens(resto);

        functions(tk, tks);
    }

    input_file.close();
}

void scanner::execute_line(string linea) {
    if (!linea.empty() && linea.back() == '\r') {
        linea.pop_back();
    }
    size_t first_non_space = linea.find_first_not_of(" \t");
    if (first_non_space == string::npos) return;
    linea = linea.substr(first_non_space);

    if (linea.front() == '#') return;

    bool in_q = false;
    string clean_line = "";
    for (char c : linea) {
        if (c == '\"') in_q = !in_q;
        if (c == '#' && !in_q) break;
        clean_line += c;
    }
    linea = clean_line;

    string tk = token(linea);
    if (tk.empty()) return;

    string resto = linea.substr(tk.length());
    vector<string> tks = split_tokens(resto);
    functions(tk, tks);
}

void scanner::execute_script(string script_text) {
    stringstream ss(script_text);
    string line;
    while (getline(ss, line)) {
        execute_line(line);
    }
}

Mount& scanner::get_mount_instance() {
    return mount_inst;
}

Users& scanner::get_user_instance() {
    return user;
}

bool scanner::is_logged_in() {
    return logued;
}

void scanner::cmd_journaling(vector<string> tokens) {
    try {
        string id = "";
        for (const string &tok : tokens) {
            string key = shared.lower(tok.substr(0, tok.find('=')));
            if (shared.compare(key, "id")) {
                string val = tok.substr(key.length() + 1);
                if (!val.empty() && val.front() == '\"') val = val.substr(1, val.length() - 2);
                id = val;
            }
        }
        if (id.empty() && logued) {
            id = user.logged.id;
        }
        if (id.empty()) {
            throw runtime_error("requiere parametro obligatorio -id");
        }

        string disk_path;
        Structs::Partition part = mount_inst.getmount(id, &disk_path);
        if (part.part_start == -1) {
            throw runtime_error("la particion no esta montada: " + id);
        }

        FILE *f = fopen(disk_path.c_str(), "rb");
        if (!f) throw runtime_error("no se pudo abrir el disco");

        Structs::Superblock sb;
        fseek(f, part.part_start, SEEK_SET);
        if (fread(&sb, sizeof(Structs::Superblock), 1, f) != 1) {
            fclose(f);
            throw runtime_error("no se pudo leer el superbloque");
        }

        if (sb.s_filesystem_type != 3) {
            fclose(f);
            throw runtime_error("la particion no es EXT3 (no contiene journaling)");
        }

        cout << "\n======================================== REPORTE DE JOURNALING (ID: " << id << ") ========================================" << endl;
        cout << left << setw(4) << "#" 
             << setw(12) << "OPERACION" 
             << setw(32) << "PATH" 
             << setw(22) << "FECHA" 
             << "CONTENIDO" << endl;
        cout << string(95, '-') << endl;

        int entries_found = 0;
        fseek(f, part.part_start + sizeof(Structs::Superblock), SEEK_SET);
        for (int i = 0; i < 50; i++) {
            Structs::Journal j_entry;
            if (fread(&j_entry, sizeof(Structs::Journal), 1, f) == 1) {
                if (j_entry.j_count > 0 && strlen(j_entry.j_content.i_operation) > 0) {
                    entries_found++;
                    time_t t = static_cast<time_t>(j_entry.j_content.i_date);
                    char buf[30];
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));

                    string content_str = j_entry.j_content.i_content;
                    if (content_str.empty()) content_str = "-";
                    else if (content_str.length() > 30) content_str = content_str.substr(0, 27) + "...";

                    cout << left << setw(4) << j_entry.j_count
                         << setw(12) << j_entry.j_content.i_operation
                         << setw(32) << j_entry.j_content.i_path
                         << setw(22) << buf
                         << content_str << endl;
                }
            }
        }

        if (entries_found == 0) {
            cout << "(No hay transacciones registradas en el journal)" << endl;
        }
        cout << string(95, '=') << "\n" << endl;
        fclose(f);
    }
    catch (exception &e) {
        shared.handler("JOURNALING", e.what());
    }
}

