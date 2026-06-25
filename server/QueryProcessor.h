#pragma once
#include <string>
#include <vector>
#include <map>
#include "TinySQLDb.h"

class QueryProcessor {
public:
    QueryProcessor(StoredDataManager& sdm);

    // Punto de entrada principal. Recibe la sentencia SQL y la base de datos activa.
    QueryResult execute(const std::string& sql, const std::string& currentDb);

private:
    StoredDataManager& sdm;

    // Tokenizer
    // Divide el SQL en tokens (palabras, paréntesis, comas, strings entre comillas)
    std::vector<std::string> tokenize(const std::string& sql);

    // Handlers por tipo de sentencia
    QueryResult handleCreateDatabase(const std::vector<std::string>& tokens);
    QueryResult handleSetDatabase   (const std::vector<std::string>& tokens);
    QueryResult handleCreateTable   (const std::vector<std::string>& tokens, const std::string& db);
    QueryResult handleDropTable     (const std::vector<std::string>& tokens, const std::string& db);
    QueryResult handleInsert        (const std::vector<std::string>& tokens, const std::string& db);
    QueryResult handleSelect        (const std::vector<std::string>& tokens, const std::string& db);
    QueryResult handleUpdate        (const std::vector<std::string>& tokens, const std::string& db);
    QueryResult handleDelete        (const std::vector<std::string>& tokens, const std::string& db);
    QueryResult handleCreateIndex   (const std::vector<std::string>& tokens, const std::string& db);

    // Utilidades
    // Convierte string a mayúsculas (para comparar keywords sin importar capitalización)
    std::string toUpper(const std::string& s);

    // Convierte string de tipo ("INTEGER", "VARCHAR(30)"...) a ColumnType + maxLength
    bool parseColumnType(const std::string& typeStr, ColumnType& outType, int& outLength);

    // Parsea el bloque WHERE en un Filter. Devuelve false si no hay WHERE.
    bool parseWhere(const std::vector<std::string>& tokens, int startIdx, Filter& outFilter);

    // Valida que un valor string sea compatible con el tipo de columna esperado
    bool validateValue(const std::string& value, ColumnType type);

    // Convierte un ColumnValue a string para mostrar en resultados
    std::string columnValueToString(const ColumnValue& cv);
};