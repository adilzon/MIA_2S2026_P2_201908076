#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <algorithm>

#include "../lib/httplib.h"
#include "../lib/json.hpp"

#include "../lib/scanner.h"
#include "../lib/mount.h"
#include "../lib/users.h"
#include "../lib/structs.h"

using namespace std;
using json = nlohmann::json;

// Helper para formatear time_t a string legible
static string format_time(time_t rawtime) {
    if (rawtime <= 0) return "";
    struct tm *timeinfo = localtime(&rawtime);
    if (!timeinfo) return "";
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return string(buffer);
}

// Helper para determinar MIME type según extensión
static string get_mime_type(const string &path) {
    filesystem::path p(path);
    string ext = p.extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".txt") return "text/plain";
    if (ext == ".json") return "application/json";
    if (ext == ".html") return "text/html";
    return "application/octet-stream";
}

int main() {
    httplib::Server svr;
    scanner scan;

    cout << "\033[1;36m====================================================\033[0m" << endl;
    cout << "\033[1;32m      Servidor HTTP EXT2/EXT3 - MIA 2S2026\033[0m" << endl;
    cout << "\033[1;36m====================================================\033[0m" << endl;

    // -------------------------------------------------------------
    // Configuración Global de CORS
    // -------------------------------------------------------------
    auto set_cors_headers = [](httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
    };

    // Preflight Handler para todas las rutas
    svr.Options(R"(.*)", [set_cors_headers](const httplib::Request &, httplib::Response &res) {
        set_cors_headers(res);
        res.status = 204;
    });

    // Helper para capturar stdout de comandos
    auto run_captured = [&](const std::function<void()>& func) -> string {
        stringstream ss;
        streambuf* prev_cout = cout.rdbuf(ss.rdbuf());
        try {
            func();
        } catch (const exception& e) {
            cout << "\n[EXCEPCION]: " << e.what() << endl;
        } catch (...) {
            cout << "\n[ERROR DESCONOCIDO]" << endl;
        }
        cout.rdbuf(prev_cout);
        return ss.str();
    };

    // -------------------------------------------------------------
    // 1. POST /api/execute
    // -------------------------------------------------------------
    svr.Post("/api/execute", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        string commands = "";

        if (req.has_header("Content-Type") && req.get_header_value("Content-Type").find("application/json") != string::npos) {
            try {
                auto body = json::parse(req.body);
                if (body.contains("commands") && body["commands"].is_string()) {
                    commands = body["commands"].get<string>();
                } else if (body.contains("command") && body["command"].is_string()) {
                    commands = body["command"].get<string>();
                } else if (body.contains("code") && body["code"].is_string()) {
                    commands = body["code"].get<string>();
                }
            } catch (...) {
                commands = req.body;
            }
        } else {
            commands = req.body;
        }

        if (commands.empty()) {
            json err_resp = {{"status", "error"}, {"message", "No se recibieron comandos para ejecutar"}};
            res.set_content(err_resp.dump(), "application/json");
            res.status = 400;
            return;
        }

        string output = run_captured([&]() {
            scan.execute_script(commands);
        });

        json ok_resp = {
            {"status", "success"},
            {"output", output}
        };
        res.set_content(ok_resp.dump(), "application/json");
    });

    // -------------------------------------------------------------
    // 2. GET /api/file
    // -------------------------------------------------------------
    svr.Get("/api/file", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        if (!req.has_param("path")) {
            json err = {{"status", "error"}, {"message", "Parametro 'path' requerido"}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        string file_path = req.get_param_value("path");
        if (!filesystem::exists(file_path)) {
            json err = {{"status", "error"}, {"message", "Archivo no encontrado: " + file_path}};
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        ifstream in(file_path, ios::binary);
        if (!in.is_open()) {
            json err = {{"status", "error"}, {"message", "No se pudo abrir el archivo"}};
            res.status = 500;
            res.set_content(err.dump(), "application/json");
            return;
        }

        string content((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
        in.close();

        string mime = get_mime_type(file_path);
        res.set_content(content, mime.c_str());
    });

    // -------------------------------------------------------------
    // 3. POST /api/login
    // -------------------------------------------------------------
    svr.Post("/api/login", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        string user = "", pass = "", id = "";

        try {
            auto body = json::parse(req.body);
            if (body.contains("user")) user = body["user"].get<string>();
            else if (body.contains("usr")) user = body["usr"].get<string>();

            if (body.contains("pass")) pass = body["pass"].get<string>();
            else if (body.contains("pwd")) pass = body["pwd"].get<string>();

            if (body.contains("id")) id = body["id"].get<string>();
        } catch (...) {
            json err = {{"status", "error"}, {"message", "Cuerpo JSON invalido"}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        if (user.empty() || pass.empty() || id.empty()) {
            json err = {{"status", "error"}, {"message", "Faltan parametros de login (user, pass, id)"}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        string cmd = "login -usr=" + user + " -pwd=" + pass + " -id=" + id;
        string output = run_captured([&]() {
            scan.execute_line(cmd);
        });

        bool logged = scanner::is_logged_in();
        json resp = {
            {"status", logged ? "success" : "error"},
            {"message", output},
            {"isLoggedIn", logged},
            {"user", user},
            {"id", id}
        };

        res.status = logged ? 200 : 401;
        res.set_content(resp.dump(), "application/json");
    });

    // -------------------------------------------------------------
    // 4. POST /api/logout
    // -------------------------------------------------------------
    svr.Post("/api/logout", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        string output = run_captured([&]() {
            scan.execute_line("logout");
        });

        bool logged = scanner::is_logged_in();
        json resp = {
            {"status", "success"},
            {"message", output},
            {"isLoggedIn", logged}
        };

        res.set_content(resp.dump(), "application/json");
    });

    // -------------------------------------------------------------
    // 5. GET /api/session
    // -------------------------------------------------------------
    svr.Get("/api/session", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        bool logged = scanner::is_logged_in();
        Users::User u = scanner::get_user_instance().logged;

        json resp = {
            {"isLoggedIn", logged},
            {"user", logged ? u.user : ""},
            {"id", logged ? u.id : ""},
            {"uid", logged ? u.uid : -1},
            {"gid", logged ? u.gid : -1},
            {"grp", logged ? u.grp : ""}
        };

        res.set_content(resp.dump(), "application/json");
    });

    // -------------------------------------------------------------
    // 6. GET /api/partitions
    // -------------------------------------------------------------
    svr.Get("/api/partitions", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        Mount& m = scanner::get_mount_instance();
        json list = json::array();

        for (int i = 0; i < 99; i++) {
            if (m.mounted[i].status == '1') {
                string path = m.mounted[i].path;
                for (int j = 0; j < 26; j++) {
                    if (m.mounted[i].mpartitions[j].status == '1') {
                        json part = {
                            {"id", string(m.mounted[i].mpartitions[j].id)},
                            {"name", string(m.mounted[i].mpartitions[j].name)},
                            {"path", path},
                            {"letter", string(1, m.mounted[i].mpartitions[j].letter)},
                            {"num", m.mounted[i].mpartitions[j].num}
                        };
                        list.push_back(part);
                    }
                }
            }
        }

        json resp = {
            {"status", "success"},
            {"partitions", list}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // -------------------------------------------------------------
    // 7. GET /api/disks
    // -------------------------------------------------------------
    svr.Get("/api/disks", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        Mount& m = scanner::get_mount_instance();
        json disks_arr = json::array();

        vector<string> disk_paths;
        if (req.has_param("path")) {
            disk_paths.push_back(req.get_param_value("path"));
        } else {
            for (int i = 0; i < 99; i++) {
                if (m.mounted[i].status == '1' && strlen(m.mounted[i].path) > 0) {
                    string p = m.mounted[i].path;
                    if (find(disk_paths.begin(), disk_paths.end(), p) == disk_paths.end()) {
                        disk_paths.push_back(p);
                    }
                }
            }
        }

        for (const string &dp : disk_paths) {
            FILE *f = fopen(dp.c_str(), "rb");
            if (!f) continue;

            Structs::MBR mbr;
            if (fread(&mbr, sizeof(Structs::MBR), 1, f) == 1) {
                json disk_obj = {
                    {"path", dp},
                    {"size", mbr.mbr_tamano},
                    {"creationDate", format_time(mbr.mbr_fecha_creacion)},
                    {"signature", mbr.mbr_disk_signature},
                    {"fit", string(1, mbr.disk_fit)},
                    {"partitions", json::array()}
                };

                auto parse_part = [](const Structs::Partition &p) -> json {
                    if (p.part_status == '0' || p.part_size <= 0) return nullptr;
                    return {
                        {"name", string(p.part_name)},
                        {"type", string(1, p.part_type)},
                        {"fit", string(1, p.part_fit)},
                        {"start", p.part_start},
                        {"size", p.part_size},
                        {"status", string(1, p.part_status)}
                    };
                };

                json p1 = parse_part(mbr.mbr_Partition_1); if (!p1.is_null()) disk_obj["partitions"].push_back(p1);
                json p2 = parse_part(mbr.mbr_Partition_2); if (!p2.is_null()) disk_obj["partitions"].push_back(p2);
                json p3 = parse_part(mbr.mbr_Partition_3); if (!p3.is_null()) disk_obj["partitions"].push_back(p3);
                json p4 = parse_part(mbr.mbr_Partition_4); if (!p4.is_null()) disk_obj["partitions"].push_back(p4);

                disks_arr.push_back(disk_obj);
            }
            fclose(f);
        }

        json resp = {
            {"status", "success"},
            {"disks", disks_arr}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // -------------------------------------------------------------
    // 8. GET /api/journal
    // -------------------------------------------------------------
    svr.Get("/api/journal", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        string id = "";
        if (req.has_param("id")) {
            id = req.get_param_value("id");
        } else if (scanner::is_logged_in()) {
            id = scanner::get_user_instance().logged.id;
        }

        if (id.empty()) {
            json err = {{"status", "error"}, {"message", "Parametro 'id' requerido o debe haber una sesion activa"}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        Mount& m = scanner::get_mount_instance();
        string disk_path;
        Structs::Partition part;
        try {
            part = m.getmount(id, &disk_path);
        } catch (...) {
            json err = {{"status", "error"}, {"message", "No se encontro la particion montada con id: " + id}};
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        FILE *f = fopen(disk_path.c_str(), "rb");
        if (!f) {
            json err = {{"status", "error"}, {"message", "No se pudo abrir el archivo de disco"}};
            res.status = 500;
            res.set_content(err.dump(), "application/json");
            return;
        }

        fseek(f, part.part_start, SEEK_SET);
        Structs::Superblock sb;
        if (fread(&sb, sizeof(Structs::Superblock), 1, f) != 1) {
            fclose(f);
            json err = {{"status", "error"}, {"message", "No se pudo leer el Superbloque"}};
            res.status = 500;
            res.set_content(err.dump(), "application/json");
            return;
        }

        if (sb.s_filesystem_type != 3) {
            fclose(f);
            json err = {{"status", "error"}, {"message", "La particion no es un sistema de archivos EXT3 (no contiene journaling)"}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        json journal_list = json::array();
        int n = sb.s_inodes_count;
        fseek(f, part.part_start + sizeof(Structs::Superblock), SEEK_SET);

        for (int i = 0; i < n; i++) {
            Structs::Journaling j_entry;
            if (fread(&j_entry, sizeof(Structs::Journaling), 1, f) == 1) {
                if (strlen(j_entry.operation) > 0) {
                    json item = {
                        {"operation", string(j_entry.operation)},
                        {"type", string(1, j_entry.type)},
                        {"path", string(j_entry.path)},
                        {"content", string(j_entry.content)},
                        {"date", format_time(j_entry.date)},
                        {"size", j_entry.size}
                    };
                    journal_list.push_back(item);
                }
            }
        }
        fclose(f);

        json resp = {
            {"status", "success"},
            {"id", id},
            {"journaling", journal_list}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // -------------------------------------------------------------
    // 9. GET /api/tree
    // -------------------------------------------------------------
    svr.Get("/api/tree", [&](const httplib::Request &req, httplib::Response &res) {
        set_cors_headers(res);
        string id = "";
        if (req.has_param("id")) {
            id = req.get_param_value("id");
        } else if (scanner::is_logged_in()) {
            id = scanner::get_user_instance().logged.id;
        }

        if (id.empty()) {
            json err = {{"status", "error"}, {"message", "Parametro 'id' requerido o debe haber una sesion activa"}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        string report_output = "reporte_tree_" + id + ".jpg";
        if (req.has_param("output")) {
            report_output = req.get_param_value("output");
        }

        string cmd = "rep -id=" + id + " -path=" + report_output + " -name=tree";
        string log_out = run_captured([&]() {
            scan.execute_line(cmd);
        });

        json resp = {
            {"status", "success"},
            {"id", id},
            {"reportPath", report_output},
            {"fileUrl", "/api/file?path=" + report_output},
            {"output", log_out}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // Handle 404
    svr.set_error_handler([set_cors_headers](const httplib::Request &, httplib::Response &res) {
        set_cors_headers(res);
        json err = {{"status", "error"}, {"message", "Endpoint no encontrado (404)"}};
        res.set_content(err.dump(), "application/json");
    });

    int port = 8080;
    cout << "\033[1;32m[SERVER HTTP]\033[0m Servidor iniciado exitosamente en http://localhost:" << port << endl;
    cout << "\033[1;33mEndpoints expuestos:\033[0m" << endl;
    cout << "  - POST http://localhost:" << port << "/api/execute" << endl;
    cout << "  - GET  http://localhost:" << port << "/api/file?path=..." << endl;
    cout << "  - POST http://localhost:" << port << "/api/login" << endl;
    cout << "  - POST http://localhost:" << port << "/api/logout" << endl;
    cout << "  - GET  http://localhost:" << port << "/api/session" << endl;
    cout << "  - GET  http://localhost:" << port << "/api/partitions" << endl;
    cout << "  - GET  http://localhost:" << port << "/api/disks" << endl;
    cout << "  - GET  http://localhost:" << port << "/api/journal?id=..." << endl;
    cout << "  - GET  http://localhost:" << port << "/api/tree?id=..." << endl;
    cout << "\033[1;36mPresiona Ctrl+C para detener el servidor.\033[0m" << endl;

    if (!svr.listen("0.0.0.0", port)) {
        cerr << "\033[1;31mError al iniciar el servidor en el puerto " << port << "\033[0m" << endl;
        return 1;
    }

    return 0;
}
