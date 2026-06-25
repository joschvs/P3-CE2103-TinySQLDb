#pragma once
#include "httplib.h"
#include "QueryProcessor.h"
#include "TinySQLDb.h"
#include <string>

class WebAPI {
public:
    WebAPI(QueryProcessor& qp, int port = 8080);

    // Arranca el servidor y se queda escuchando
    void run();

private:
    QueryProcessor& qp;
    httplib::Server server;
    int port;

    // El contexto de BD activa se guarda aquí
    // (en una app real sería por sesión, pero para este proyecto es global)
    std::string currentDb;

    // Registra todos los endpoints
    void setupRoutes();

    // Convierte un QueryResult a string JSON para devolver al cliente
    std::string toJson(const QueryResult& result, double elapsedMs);

    // Extrae el valor de un campo string de un JSON simple
    std::string extractJsonField(const std::string& json, const std::string& field);

    // Escapa caracteres especiales para JSON válido
    std::string escapeJson(const std::string& s);
};