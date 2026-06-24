#include "QueryProcessor.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>


// Constructor

QueryProcessor::QueryProcessor(StoredDataManager& sdm) : sdm(sdm) {}

// EXECUTE: punto de entrada principal
// Recibe el SQL crudo y la BD activa, identifica qué sentencia es y delega.

QueryResult QueryProcessor::execute(const std::string& sql, const std::string& currentDb) {
    std::vector<std::string> tokens = tokenize(sql);
    if (tokens.empty()) {
        return {false, "Sentencia vacía.", {}, {}};
    }

    std::string first = toUpper(tokens[0]);

    if (first == "CREATE") {
        if (tokens.size() < 2) return {false, "Sintaxis incorrecta después de CREATE.", {}, {}};
        std::string second = toUpper(tokens[1]);
        if (second == "DATABASE") return handleCreateDatabase(tokens);
        if (second == "TABLE")    return handleCreateTable(tokens, currentDb);
        if (second == "INDEX")    return handleCreateIndex(tokens, currentDb);
        return {false, "CREATE no reconocido: " + tokens[1], {}, {}};
    }
    if (first == "SET")    return handleSetDatabase(tokens);
    if (first == "DROP")   return handleDropTable(tokens, currentDb);
    if (first == "INSERT") return handleInsert(tokens, currentDb);
    if (first == "SELECT") return handleSelect(tokens, currentDb);
    if (first == "UPDATE") return handleUpdate(tokens, currentDb);
    if (first == "DELETE") return handleDelete(tokens, currentDb);

    return {false, "Sentencia no reconocida: " + tokens[0], {}, {}};
}

// TOKENIZER
// Convierte el string SQL en una lista de tokens:
//   - palabras separadas por espacios
//   - strings entre comillas dobles = un solo token (sin las comillas)
//   - '(' ')' ',' ';' ada uno es su propio token

std::vector<std::string> QueryProcessor::tokenize(const std::string& sql) {
    std::vector<std::string> tokens;
    std::string current;
    bool inString = false;

    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];

        if (c == '"') {
            inString = !inString;
            // Al cerrar comillas, guarda lo acumulado como token
            if (!inString && !current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        if (inString) {
            current += c;
            continue;
        }

        if (c == '(' || c == ')' || c == ',' || c == ';') {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
            tokens.push_back(std::string(1, c));
            continue;
        }

        if (std::isspace(c)) {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
            continue;
        }

        current += c;
    }

    if (!current.empty()) tokens.push_back(current);
    return tokens;
}


// UTILIDADES

// Convierte un string a mayúsculas para comparar keywords sin importar capitalización
std::string QueryProcessor::toUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::toupper);
    return out;
}

// Convierte "INTEGER", "DOUBLE", "DATETIME", "VARCHAR" al enum ColumnType correcto
bool QueryProcessor::parseColumnType(const std::string& typeStr, ColumnType& outType, int& outLength) {
    std::string up = toUpper(typeStr);
    outLength = 0;

    if (up == "INTEGER")  { outType = ColumnType::INTEGER;  return true; }
    if (up == "DOUBLE")   { outType = ColumnType::DOUBLE;   return true; }
    if (up == "DATETIME") { outType = ColumnType::DATETIME; return true; }
    if (up == "VARCHAR")  { outType = ColumnType::VARCHAR;  return true; }

    return false; // tipo no reconocido
}

// Verifica que un valor string sea compatible con el tipo de columna esperado
bool QueryProcessor::validateValue(const std::string& value, ColumnType type) {
    if (type == ColumnType::INTEGER) {
        try { std::stoi(value); return true; } catch (...) { return false; }
    }
    if (type == ColumnType::DOUBLE) {
        try { std::stod(value); return true; } catch (...) { return false; }
    }
    // VARCHAR y DATETIME aceptan cualquier string; el SDM hace validaciones más finas
    return true;
}

// Parsea el bloque WHERE a partir del índice dado
// Espera exactamente: WHERE <columna> <operador> <valor>
bool QueryProcessor::parseWhere(const std::vector<std::string>& tokens, int startIdx, Filter& outFilter) {
    if (startIdx < 0 || startIdx >= (int)tokens.size()) return false;
    if (toUpper(tokens[startIdx]) != "WHERE") return false;
    if ((int)tokens.size() < startIdx + 4) return false;

    outFilter.column = tokens[startIdx + 1];
    outFilter.op     = tokens[startIdx + 2]; // =, >, <, like, not
    outFilter.value  = tokens[startIdx + 3];
    return true;
}

// Convierte un ColumnValue a string para mostrarlo en los resultados del SELECT
std::string QueryProcessor::columnValueToString(const ColumnValue& cv) {
    return cv.raw;
}

// CREATE DATABASE <nombre>

QueryResult QueryProcessor::handleCreateDatabase(const std::vector<std::string>& tokens) {
    // tokens: CREATE DATABASE <nombre>
    if (tokens.size() < 3) {
        return {false, "Sintaxis: CREATE DATABASE <nombre>", {}, {}};
    }

    std::string dbName = tokens[2];

    if (sdm.databaseExists(dbName)) {
        return {false, "La base de datos '" + dbName + "' ya existe.", {}, {}};
    }

    sdm.createDatabase(dbName);
    return {true, "Base de datos '" + dbName + "' creada exitosamente.", {}, {}};
}


// SET DATABASE <nombre>
// El QP solo valida que la BD exista. El cliente y el Web API guardan el contexto.
// Para comunicarle el nombre al API, se devuelve en el mensaje con prefijo "DATABASE:"


QueryResult QueryProcessor::handleSetDatabase(const std::vector<std::string>& tokens) {
    // tokens: SET DATABASE <nombre>
    if (tokens.size() < 3) {
        return {false, "Sintaxis: SET DATABASE <nombre>", {}, {}};
    }

    std::string dbName = tokens[2];

    if (!sdm.databaseExists(dbName)) {
        return {false, "La base de datos '" + dbName + "' no existe.", {}, {}};
    }

    return {true, "DATABASE:" + dbName, {}, {}};
}