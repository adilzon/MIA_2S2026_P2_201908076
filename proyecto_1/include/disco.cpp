#include "../lib/disco.h"
#include "../lib/structs.h"
#include "../lib/scanner.h"

#include <vector>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <algorithm>
#include <filesystem>

using namespace std;

Disk::Disk() {}

void Disk::mkdisk(vector<string> tokens)
{
    string size = "";
    string f = "";
    string u = "";
    string path = "";
    bool error = false;

    for (string token : tokens)
    {
        string tk = token.substr(0, token.find("="));
        token.erase(0, tk.length() + 1);
        if (token.front() == '\"') {
            token = token.substr(1, token.length() - 2);
        }

        if (shared.compare(tk, "f") || shared.compare(tk, "fit"))
        {
            if (f.empty()) {
                f = token;
            } else {
                shared.handler("MKDISK", "parametro FIT repetido en el comando " + tk);
                error = true;
            }
        }
        else if (shared.compare(tk, "size"))
        {
            if (size.empty()) {
                size = token;
            } else {
                shared.handler("MKDISK", "parametro SIZE repetido en el comando " + tk);
                error = true;
            }
        }
        else if (shared.compare(tk, "u") || shared.compare(tk, "unit"))
        {
            if (u.empty()) {
                u = token;
            } else {
                shared.handler("MKDISK", "parametro UNIT repetido en el comando " + tk);
                error = true;
            }
        }
        else if (shared.compare(tk, "path"))
        {
            if (path.empty()) {
                path = token;
            } else {
                shared.handler("MKDISK", "parametro PATH repetido en el comando " + tk);
                error = true;
            }
        }
        else
        {
            shared.handler("MKDISK", "no se esperaba el parametro " + tk);
            error = true;
            break;
        }
    }

    if (error) return;

    if (f.empty()) f = "FF";
    if (u.empty()) u = "M";

    if (path.empty() && size.empty()) {
        shared.handler("MKDISK", "Obligatorio el parametro Path y Size");
    } else if (path.empty()) {
        shared.handler("MKDISK", "Obligatorio el parametro Path");
    } else if (size.empty()) {
        shared.handler("MKDISK", "Obligatorio el parametro Size para este comando");
    } else if (!shared.compare(f, "BF") && !shared.compare(f, "FF") && !shared.compare(f, "WF")) {
        shared.handler("MKDISK", "valores de parametro FIT no válidos (BF, FF, WF)");
    } else if (!shared.compare(u, "k") && !shared.compare(u, "m")) {
        shared.handler("MKDISK", "valores de parametro UNIT no válidos (k, m)");
    } else {
        makeDisk(size, f, u, path);
    }
}

void Disk::makeDisk(string s, string f, string u, string path)
{
    Structs::MBR disco;
    try
    {
        int size = stoi(s);
        if (size <= 0) {
            shared.handler("MKDISK", "size debe ser mayor a 0");
            return;
        }
        if (shared.compare(u, "M")) {
            size = 1024 * 1024 * size;
        } else if (shared.compare(u, "K")) {
            size = 1024 * size;
        }

        disco.mbr_tamano = size;
        disco.mbr_fecha_creacion = time(nullptr);
        disco.disk_fit = toupper(f[0]);
        disco.mbr_disk_signature = rand() % 9999 + 100;
        disco.mbr_Partition_1 = Structs::Partition();
        disco.mbr_Partition_2 = Structs::Partition();
        disco.mbr_Partition_3 = Structs::Partition();
        disco.mbr_Partition_4 = Structs::Partition();

        if (path.front() == '\"') {
            path = path.substr(1, path.length() - 2);
        }

        // Crear directorios si no existen
        try {
            filesystem::path fp(path);
            if (fp.has_parent_path()) {
                filesystem::create_directories(fp.parent_path());
            }
        } catch (...) {}

        FILE *file = fopen(path.c_str(), "wb+");
        if (file == NULL) {
            shared.handler("MKDISK", "Error al crear el disco en la ruta indicada");
            return;
        }

        // Inicializar archivo
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        int total_written = 0;
        while (total_written < size) {
            int to_write = min((int)sizeof(buffer), size - total_written);
            fwrite(buffer, 1, to_write, file);
            total_written += to_write;
        }

        rewind(file);
        fwrite(&disco, sizeof(Structs::MBR), 1, file);
        fclose(file);

        struct tm *tm = localtime(&disco.mbr_fecha_creacion);
        char mostrar_fecha[30];
        strftime(mostrar_fecha, 30, "%Y/%m/%d %H:%M:%S", tm);

        shared.response("MKDISK", "Disco creado exitosamente en: " + path);
        cout << "  Tamaño: " << disco.mbr_tamano << " bytes (" << s << u << ")" << endl;
        cout << "  Fecha: " << mostrar_fecha << endl;
        cout << "  Fit: " << disco.disk_fit << endl;
        cout << "  Signature: " << disco.mbr_disk_signature << endl;
    }
    catch (invalid_argument &e) {
        shared.handler("MKDISK", "size debe ser un entero");
    }
    catch (exception &e) {
        shared.handler("MKDISK", "Error al crear el disco: " + string(e.what()));
    }
}

void Disk::rmdisk(vector<string> context)
{
    string path = "";
    if (context.empty()) {
        shared.handler("RMDISK", "Se esperaba el path para completar la acción");
        return;
    }

    for (string token : context)
    {
        string tk = token.substr(0, token.find("="));
        token.erase(0, tk.length() + 1);
        if (token.front() == '\"') {
            token = token.substr(1, token.length() - 2);
        }

        if (shared.compare(tk, "path")) {
            path = token;
        } else {
            shared.handler("RMDISK", "No se reconoce este elemento " + tk);
            return;
        }
    }

    if (path.empty()) {
        shared.handler("RMDISK", "Path no puede estar vacío");
        return;
    }

    if (path.front() == '\"') {
        path = path.substr(1, path.length() - 2);
    }

    FILE *file = fopen(path.c_str(), "rb");
    if (file == NULL) {
        shared.handler("RMDISK", "El disco que desea eliminar no existe en la ruta indicada");
        return;
    }
    fclose(file);

    if (remove(path.c_str()) == 0) {
        shared.response("RMDISK", "Disco eliminado correctamente: " + path);
    } else {
        shared.handler("RMDISK", "Error al intentar eliminar el archivo del disco");
    }
}

void Disk::fdisk(vector<string> context)
{
    bool dlt = false;
    for (string current : context) {
        string id = shared.lower(current.substr(0, current.find('=')));
        if (shared.compare(id, "delete")) {
            dlt = true;
            break;
        }
    }

    if (!dlt) {
        vector<string> required = {"size", "path", "name"};
        string size;
        string u = "k";
        string path;
        string type = "P";
        string f = "WF";
        string name;
        string add;

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            current.erase(0, id.length() + 1);
            if (current.front() == '\"') {
                current = current.substr(1, current.length() - 2);
            }

            if (shared.compare(id, "size")) {
                auto itr = find(required.begin(), required.end(), "size");
                if (itr != required.end()) required.erase(itr);
                size = current;
            } else if (shared.compare(id, "u") || shared.compare(id, "unit")) {
                u = current;
            } else if (shared.compare(id, "path")) {
                auto itr = find(required.begin(), required.end(), "path");
                if (itr != required.end()) required.erase(itr);
                path = current;
            } else if (shared.compare(id, "type") || shared.compare(id, "t")) {
                type = current;
            } else if (shared.compare(id, "f") || shared.compare(id, "fit")) {
                f = current;
            } else if (shared.compare(id, "name")) {
                auto itr = find(required.begin(), required.end(), "name");
                if (itr != required.end()) required.erase(itr);
                name = current;
            } else if (shared.compare(id, "add")) {
                add = current;
                auto itr = find(required.begin(), required.end(), "size");
                if (itr != required.end()) required.erase(itr);
            }
        }

        if (!required.empty()) {
            shared.handler("FDISK", "create requiere parámetros obligatorios (-size, -path, -name)");
            return;
        }

        if (add.empty()) {
            generatepartition(size, u, path, type, f, name, add);
        } else {
            addpartition(add, u, name, path);
        }
    } else {
        vector<string> required = {"path", "name", "delete"};
        string _delete;
        string path;
        string name;

        for (auto current : context) {
            string id = shared.lower(current.substr(0, current.find('=')));
            current.erase(0, id.length() + 1);
            if (current.front() == '\"') {
                current = current.substr(1, current.length() - 2);
            }

            if (shared.compare(id, "path")) {
                auto itr = find(required.begin(), required.end(), "path");
                if (itr != required.end()) required.erase(itr);
                path = current;
            } else if (shared.compare(id, "name")) {
                auto itr = find(required.begin(), required.end(), "name");
                if (itr != required.end()) required.erase(itr);
                name = current;
            } else if (shared.compare(id, "delete")) {
                auto itr = find(required.begin(), required.end(), "delete");
                if (itr != required.end()) required.erase(itr);
                _delete = current;
            }
        }

        if (!required.empty()) {
            shared.handler("FDISK", "delete requiere parámetros obligatorios (-path, -name, -delete)");
            return;
        }
        deletepartition(_delete, path, name);
    }
}

void Disk::generatepartition(string s, string u, string p, string t, string f, string n, string a)
{
    try
    {
        int part_size_bytes = stoi(s);
        if (part_size_bytes <= 0) {
            throw runtime_error("-size debe de ser mayor que 0");
        }

        if (shared.compare(u, "b")) {
            // bytes
        } else if (shared.compare(u, "k")) {
            part_size_bytes *= 1024;
        } else if (shared.compare(u, "m")) {
            part_size_bytes *= 1024 * 1024;
        } else {
            throw runtime_error("-unit no contiene valores válidos (b, k, m)");
        }

        if (p.front() == '\"') {
            p = p.substr(1, p.length() - 2);
        }

        if (!shared.compare(t, "p") && !shared.compare(t, "e") && !shared.compare(t, "l")) {
            throw runtime_error("-type no contiene valores válidos (P, E, L)");
        }
        if (!shared.compare(f, "bf") && !shared.compare(f, "ff") && !shared.compare(f, "wf")) {
            throw runtime_error("-fit no contiene valores válidos (BF, FF, WF)");
        }

        FILE *file = fopen(p.c_str(), "rb+");
        if (file == NULL) {
            shared.handler("FDISK", "Este disco no existe :c");
            return;
        }

        Structs::MBR disco;
        fread(&disco, sizeof(disco), 1, file);

        vector<Structs::Partition> partitions = getPartitions(disco);
        int used = 0;
        int ext = 0;
        Structs::Partition extended;

        for (const auto &prttn : partitions) {
            if (prttn.part_status == '1') {
                used++;
                if (prttn.part_type == 'E' || prttn.part_type == 'e') {
                    ext++;
                    extended = prttn;
                }
            }
        }

        // Validar límites
        if (!shared.compare(t, "l")) {
            if (used >= 4) {
                fclose(file);
                throw runtime_error("Limite de particiones primarias/extendidas alcanzado (máximo 4)");
            }
            if (ext >= 1 && shared.compare(t, "e")) {
                fclose(file);
                throw runtime_error("Solo se puede crear una particion Extendida :c");
            }
        } else {
            if (ext == 0) {
                fclose(file);
                throw runtime_error("No existe particion Extendida para crear la Logica");
            }
        }

        // Validar nombre único
        try {
            findby(disco, n, p);
            fclose(file);
            shared.handler("FDISK", "Este nombre ya esta en uso");
            return;
        } catch (...) {}

        // Partición Lógica
        if (shared.compare(t, "l")) {
            fclose(file);
            Structs::Partition logic_part;
            logic_part.part_status = '1';
            logic_part.part_size = part_size_bytes;
            logic_part.part_type = 'L';
            logic_part.part_fit = toupper(f[0]);
            strncpy(logic_part.part_name, n.c_str(), sizeof(logic_part.part_name) - 1);
            logic(logic_part, extended, p);
            return;
        }

        // Partición Primaria o Extendida: Encontrar espacio libre
        struct Interval {
            int start;
            int size;
        };

        // Recolectar intervalos ocupados
        vector<pair<int, int>> occupied;
        for (const auto &prttn : partitions) {
            if (prttn.part_status == '1') {
                occupied.push_back({prttn.part_start, prttn.part_start + prttn.part_size});
            }
        }
        sort(occupied.begin(), occupied.end());

        // Generar huecos libres
        vector<Interval> free_spaces;
        int current_pos = sizeof(Structs::MBR);

        for (const auto &occ : occupied) {
            if (occ.first > current_pos) {
                free_spaces.push_back({current_pos, occ.first - current_pos});
            }
            current_pos = max(current_pos, occ.second);
        }
        if (current_pos < disco.mbr_tamano) {
            free_spaces.push_back({current_pos, disco.mbr_tamano - current_pos});
        }

        // Filtrar huecos que caben
        vector<Interval> candidates;
        for (const auto &sp : free_spaces) {
            if (sp.size >= part_size_bytes) {
                candidates.push_back(sp);
            }
        }

        if (candidates.empty()) {
            fclose(file);
            throw runtime_error("No hay suficiente espacio en el disco para la partición");
        }

        // Aplicar ajuste (FF, BF, WF) según disco.disk_fit
        Interval chosen = candidates[0];
        char disk_fit = toupper(disco.disk_fit);

        if (disk_fit == 'B') {
            for (const auto &c : candidates) {
                if (c.size < chosen.size) chosen = c;
            }
        } else if (disk_fit == 'W') {
            for (const auto &c : candidates) {
                if (c.size > chosen.size) chosen = c;
            }
        } // default FF: candidates[0]

        // Crear partición
        Structs::Partition new_part;
        new_part.part_status = '1';
        new_part.part_type = toupper(t[0]);
        new_part.part_fit = toupper(f[0]);
        new_part.part_start = chosen.start;
        new_part.part_size = part_size_bytes;
        memset(new_part.part_name, 0, sizeof(new_part.part_name));
        strncpy(new_part.part_name, n.c_str(), sizeof(new_part.part_name) - 1);

        // Guardar en primer slot libre de MBR
        if (disco.mbr_Partition_1.part_status == '0') {
            disco.mbr_Partition_1 = new_part;
        } else if (disco.mbr_Partition_2.part_status == '0') {
            disco.mbr_Partition_2 = new_part;
        } else if (disco.mbr_Partition_3.part_status == '0') {
            disco.mbr_Partition_3 = new_part;
        } else if (disco.mbr_Partition_4.part_status == '0') {
            disco.mbr_Partition_4 = new_part;
        }

        rewind(file);
        fwrite(&disco, sizeof(Structs::MBR), 1, file);

        // Si es extendida, inicializar EBR en su inicio
        if (shared.compare(t, "e")) {
            Structs::EBR ebr;
            ebr.part_status = '0';
            ebr.part_fit = toupper(f[0]);
            ebr.part_start = new_part.part_start;
            ebr.part_size = 0;
            ebr.part_next = -1;
            memset(ebr.part_name, 0, sizeof(ebr.part_name));
            fseek(file, new_part.part_start, SEEK_SET);
            fwrite(&ebr, sizeof(Structs::EBR), 1, file);
        }

        fclose(file);
        shared.response("FDISK", "partición creada correctamente: " + n);
    }
    catch (invalid_argument &e) {
        shared.handler("FDISK", "-size debe ser un entero");
    }
    catch (exception &e) {
        shared.handler("FDISK", e.what());
    }
}

vector<Structs::Partition> Disk::getPartitions(Structs::MBR disco) {
    vector<Structs::Partition> v;
    v.push_back(disco.mbr_Partition_1);
    v.push_back(disco.mbr_Partition_2);
    v.push_back(disco.mbr_Partition_3);
    v.push_back(disco.mbr_Partition_4);
    return v;
}

Structs::Partition Disk::findby(Structs::MBR mbr, string name, string path) {
    Structs::Partition partitions[4];
    partitions[0] = mbr.mbr_Partition_1;
    partitions[1] = mbr.mbr_Partition_2;
    partitions[2] = mbr.mbr_Partition_3;
    partitions[3] = mbr.mbr_Partition_4;

    bool ext = false;
    Structs::Partition extended;
    for (auto &partition : partitions) {
        if (partition.part_status == '1') {
            if (shared.compare(partition.part_name, name)) {
                return partition;
            } else if (partition.part_type == 'E' || partition.part_type == 'e') {
                ext = true;
                extended = partition;
            }
        }
    }
    if (ext) {
        vector<Structs::EBR> ebrs = getlogics(extended, path);
        for (const Structs::EBR &ebr : ebrs) {
            if (ebr.part_status == '1') {
                if (shared.compare(ebr.part_name, name)) {
                    Structs::Partition tmp;
                    tmp.part_status = '1';
                    tmp.part_type = 'L';
                    tmp.part_fit = ebr.part_fit;
                    tmp.part_start = ebr.part_start;
                    tmp.part_size = ebr.part_size;
                    strncpy(tmp.part_name, ebr.part_name, sizeof(tmp.part_name) - 1);
                    return tmp;
                }
            }
        }
    }
    throw runtime_error("la partición no existe");
}

void Disk::logic(Structs::Partition partition, Structs::Partition ep, string p) {
    FILE *file = fopen(p.c_str(), "rb+");
    if (file == NULL) {
        shared.handler("FDISK", "error al abrir el disco para crear partición lógica");
        return;
    }

    Structs::EBR tmp;
    fseek(file, ep.part_start, SEEK_SET);
    fread(&tmp, sizeof(Structs::EBR), 1, file);

    // Si el primer EBR está vacío
    if (tmp.part_status == '0' && tmp.part_next == -1) {
        if (partition.part_size + (int)sizeof(Structs::EBR) > ep.part_size) {
            fclose(file);
            shared.handler("FDISK", "no hay espacio para la partición lógica");
            return;
        }
        tmp.part_status = '1';
        tmp.part_fit = partition.part_fit;
        tmp.part_start = ep.part_start;
        tmp.part_size = partition.part_size;
        tmp.part_next = -1;
        strncpy(tmp.part_name, partition.part_name, sizeof(tmp.part_name) - 1);

        fseek(file, ep.part_start, SEEK_SET);
        fwrite(&tmp, sizeof(Structs::EBR), 1, file);
        fclose(file);
        shared.response("FDISK", "partición lógica creada correctamente");
        return;
    }

    // Recorrer hasta el último EBR
    int current_ebr_pos = ep.part_start;
    while (tmp.part_next != -1) {
        current_ebr_pos = tmp.part_next;
        fseek(file, current_ebr_pos, SEEK_SET);
        fread(&tmp, sizeof(Structs::EBR), 1, file);
    }

    int new_ebr_pos = tmp.part_start + sizeof(Structs::EBR) + tmp.part_size;
    if (new_ebr_pos + (int)sizeof(Structs::EBR) + partition.part_size > ep.part_start + ep.part_size) {
        fclose(file);
        shared.handler("FDISK", "no hay espacio para más particiones lógicas");
        return;
    }

    // Actualizar puntero next del EBR anterior
    tmp.part_next = new_ebr_pos;
    fseek(file, current_ebr_pos, SEEK_SET);
    fwrite(&tmp, sizeof(Structs::EBR), 1, file);

    // Escribir nuevo EBR
    Structs::EBR new_ebr;
    new_ebr.part_status = '1';
    new_ebr.part_fit = partition.part_fit;
    new_ebr.part_start = new_ebr_pos;
    new_ebr.part_size = partition.part_size;
    new_ebr.part_next = -1;
    strncpy(new_ebr.part_name, partition.part_name, sizeof(new_ebr.part_name) - 1);

    fseek(file, new_ebr_pos, SEEK_SET);
    fwrite(&new_ebr, sizeof(Structs::EBR), 1, file);
    fclose(file);
    shared.response("FDISK", "partición lógica creada correctamente");
}

vector<Structs::EBR> Disk::getlogics(Structs::Partition partition, string p) {
    vector<Structs::EBR> ebrs;
    FILE *file = fopen(p.c_str(), "rb");
    if (file == NULL) return ebrs;

    Structs::EBR tmp;
    fseek(file, partition.part_start, SEEK_SET);
    if (fread(&tmp, sizeof(Structs::EBR), 1, file) != 1) {
        fclose(file);
        return ebrs;
    }

    while (true) {
        if (tmp.part_status == '1') {
            ebrs.push_back(tmp);
        }
        if (tmp.part_next == -1) break;
        fseek(file, tmp.part_next, SEEK_SET);
        if (fread(&tmp, sizeof(Structs::EBR), 1, file) != 1) break;
    }

    fclose(file);
    return ebrs;
}

void Disk::deletepartition(string d, string p, string n) {
    try {
        if (p.front() == '\"') {
            p = p.substr(1, p.length() - 2);
        }

        if (!shared.compare(d, "fast") && !shared.compare(d, "full")) {
            throw runtime_error("-delete necesita valores específicos (Fast, Full)");
        }

        FILE *file = fopen(p.c_str(), "rb+");
        if (file == NULL) {
            throw runtime_error("disco no existente");
        }

        Structs::MBR disk;
        fread(&disk, sizeof(Structs::MBR), 1, file);

        Structs::Partition *parts[4] = {
            &disk.mbr_Partition_1,
            &disk.mbr_Partition_2,
            &disk.mbr_Partition_3,
            &disk.mbr_Partition_4
        };

        bool found = false;
        for (int i = 0; i < 4; i++) {
            if (parts[i]->part_status == '1' && shared.compare(parts[i]->part_name, n)) {
                found = true;
                if (shared.compare(d, "fast")) {
                    parts[i]->part_status = '0';
                } else {
                    int start = parts[i]->part_start;
                    int size = parts[i]->part_size;
                    *parts[i] = Structs::Partition();
                    fseek(file, start, SEEK_SET);
                    char zero = '\0';
                    for (int z = 0; z < size; z++) fwrite(&zero, 1, 1, file);
                }
                break;
            }
        }

        if (!found) {
            fclose(file);
            throw runtime_error("la partición no existe en el disco");
        }

        rewind(file);
        fwrite(&disk, sizeof(Structs::MBR), 1, file);
        fclose(file);
        shared.response("FDISK", "partición eliminada correctamente");
    }
    catch (exception &e) {
        shared.handler("FDISK", e.what());
    }
}

void Disk::addpartition(string add, string u, string n, string p) {
    // Add or remove space
}