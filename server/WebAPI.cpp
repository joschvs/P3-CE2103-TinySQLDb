#include "WebAPI.h"
#include <chrono>
#include <sstream>


// Constructor

WebAPI::WebAPI(QueryProcessor& qp, int port) : qp(qp), port(port), currentDb("") {
    setupRoutes();
}

// RUN — arranca el servidor

void WebAPI::run() {
    std::cout << "TinySQLDb server corriendo en http://localhost:" << port << "\n";
    server.listen("0.0.0.0", port);
}

// SETUP ROUTES — registra los endpoints

void WebAPI::setupRoutes() {

    // CORS: permite que el cliente React se conecte desde otro puerto 
    server.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // POST /query — ejecuta una sentencia SQL 
    // Body JSON esperado: { "sql": "SELECT * FROM ...", "database": "Universidad" }
    // Respuesta JSON:     { "success": true, "message": "...", "columns": [...], "rows": [...], "elapsedMs": 1.23 }
    server.Post("/query", [this](const httplib::Request& req, httplib::Response& res) {

        // Extraer el campo "sql" del JSON manualmente (sin librería JSON externa)
        std::string sql      = extractJsonField(req.body, "sql");
        std::string database = extractJsonField(req.body, "database");

        // Si el cliente manda una BD activa, usarla; si no, usar la guardada
        if (!database.empty()) currentDb = database;

        if (sql.empty()) {
            res.set_content("{\"success\":false,\"message\":\"Campo sql vacío.\",\"columns\":[],\"rows\":[],\"elapsedMs\":0}", "application/json");
            return;
        }

        //  Medir tiempo y ejecutar
        auto start = std::chrono::high_resolution_clock::now();
        QueryResult result = qp.execute(sql, currentDb);
        auto end   = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        //  Si fue SET DATABASE exitoso, actualizar contexto
        if (result.success && result.message.rfind("DATABASE:", 0) == 0) {
            currentDb = result.message.substr(9);
        }

        //  Devolver JSON
        res.set_content(toJson(result, elapsedMs), "application/json");
    });

    // GET /status — health check para saber si el servidor está vivo 
    server.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
}


// TO JSON — convierte QueryResult a string JSON

std::string WebAPI::toJson(const QueryResult& result, double elapsedMs) {
    std::ostringstream json;

    json << "{";
    json << "\"success\":" << (result.success ? "true" : "false") << ",";
    json << "\"message\":\"" << escapeJson(result.message) << "\",";
    json << "\"elapsedMs\":" << elapsedMs << ",";

    // Columnas
    json << "\"columns\":[";
    for (size_t i = 0; i < result.columns.size(); ++i) {
        json << "\"" << escapeJson(result.columns[i]) << "\"";
        if (i + 1 < result.columns.size()) json << ",";
    }
    json << "],";

    // Filas
    json << "\"rows\":[";
    for (size_t i = 0; i < result.rows.size(); ++i) {
        json << "[";
        for (size_t j = 0; j < result.rows[i].size(); ++j) {
            json << "\"" << escapeJson(result.rows[i][j]) << "\"";
            if (j + 1 < result.rows[i].size()) json << ",";
        }
        json << "]";
        if (i + 1 < result.rows.size()) json << ",";
    }
    json << "]";

    json << "}";
    return json.str();
}


// Extrae el valor de un campo de un JSON simple sin librería externa
// Solo funciona para valores string: { "campo": "valor" }
std::string WebAPI::extractJsonField(const std::string& json, const std::string& field) {
    std::string key = "\"" + field + "\"";
    size_t keyPos = json.find(key);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(":", keyPos + key.size());
    if (colonPos == std::string::npos) return "";

    size_t quoteOpen = json.find("\"", colonPos + 1);
    if (quoteOpen == std::string::npos) return "";

    size_t quoteClose = quoteOpen + 1;
    while (quoteClose < json.size()) {
        if (json[quoteClose] == '\\') { quoteClose += 2; continue; } 
        if (json[quoteClose] == '"')  { break; }
        quoteClose++;
    }

    return json.substr(quoteOpen + 1, quoteClose - quoteOpen - 1);
}

// Escapa caracteres especiales para JSON válido
std::string WebAPI::escapeJson(const std::string& s) {
    std::ostringstream out;
    for (char c : s) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:   out << c;      break;
        }
    }
    return out.str();
}