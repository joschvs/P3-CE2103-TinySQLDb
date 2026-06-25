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

// CREATE TABLE <nombre> AS ( col type [nullability], ... )
 
QueryResult QueryProcessor::handleCreateTable(const std::vector<std::string>& tokens, const std::string& db) {
    if (db.empty()) return {false, "No hay base de datos activa. Use SET DATABASE.", {}, {}};
    if (tokens.size() < 4) return {false, "Sintaxis: CREATE TABLE <nombre> (<columnas>)", {}, {}};
 
    std::string tableName = tokens[2];
 
    // AS es opcional según el spec, saltarlo si está
    int idx = 3;
    if (toUpper(tokens[idx]) == "AS") idx++;
    if (tokens[idx] != "(") return {false, "Se esperaba '(' después del nombre de tabla.", {}, {}};
    idx++;
 
    if (sdm.tableExists(db, tableName)) {
        return {false, "La tabla '" + tableName + "' ya existe en '" + db + "'.", {}, {}};
    }
 
    // Parsear columnas hasta encontrar ')'
    std::vector<ColumnDefinition> columns;
 
    while (idx < (int)tokens.size() && tokens[idx] != ")") {
        if (tokens[idx] == ",") { idx++; continue; }
 
        ColumnDefinition col;
        col.nullable  = true;
        col.maxLength = 0;
 
        // Nombre de columna
        col.name = tokens[idx++];
        if (idx >= (int)tokens.size()) return {false, "Definición incompleta para '" + col.name + "'.", {}, {}};
 
        // Tipo
        std::string typeToken = tokens[idx++];
        if (!parseColumnType(typeToken, col.type, col.maxLength)) {
            return {false, "Tipo no reconocido: '" + typeToken + "'.", {}, {}};
        }
 
        // VARCHAR requiere tamaño: VARCHAR ( n )
        if (col.type == ColumnType::VARCHAR) {
            if (idx < (int)tokens.size() && tokens[idx] == "(") {
                idx++; // saltar '('
                if (idx >= (int)tokens.size()) return {false, "Falta tamaño de VARCHAR.", {}, {}};
                try { col.maxLength = std::stoi(tokens[idx++]); } catch (...) {
                    return {false, "Tamaño de VARCHAR inválido.", {}, {}};
                }
                if (idx < (int)tokens.size() && tokens[idx] == ")") idx++; // saltar ')'
            } else {
                return {false, "VARCHAR requiere tamaño: VARCHAR(n).", {}, {}};
            }
        }
 
        // Nullability opcional: NOT NULL | NULL
        if (idx < (int)tokens.size()) {
            std::string maybeNot = toUpper(tokens[idx]);
            if (maybeNot == "NOT") {
                idx++;
                if (idx < (int)tokens.size() && toUpper(tokens[idx]) == "NULL") {
                    col.nullable = false;
                    idx++;
                }
            } else if (maybeNot == "NULL") {
                col.nullable = true;
                idx++;
            }
        }
 
        columns.push_back(col);
    }
 
    if (columns.empty()) return {false, "La tabla debe tener al menos una columna.", {}, {}};
 
    sdm.createTable(db, tableName, columns);
    return {true, "Tabla '" + tableName + "' creada exitosamente.", {}, {}};
}
 

// DROP TABLE <nombre>
// Solo se puede eliminar si la tabla está vacía (el SDM lanza excepción si no)
 
QueryResult QueryProcessor::handleDropTable(const std::vector<std::string>& tokens, const std::string& db) {
    if (db.empty()) return {false, "No hay base de datos activa.", {}, {}};
    if (tokens.size() < 3) return {false, "Sintaxis: DROP TABLE <nombre>", {}, {}};
 
    std::string tableName = tokens[2];
 
    if (!sdm.tableExists(db, tableName)) {
        return {false, "La tabla '" + tableName + "' no existe.", {}, {}};
    }
 
    try {
        sdm.dropTable(db, tableName);
    } catch (const std::exception& e) {
        return {false, std::string("No se pudo eliminar la tabla: ") + e.what(), {}, {}};
    }
 
    return {true, "Tabla '" + tableName + "' eliminada.", {}, {}};
}
 

// INSERT INTO <tabla> VALUES( v1, v2, ... )
 
QueryResult QueryProcessor::handleInsert(const std::vector<std::string>& tokens, const std::string& db) {
    if (db.empty()) return {false, "No hay base de datos activa.", {}, {}};
    if (tokens.size() < 5) return {false, "Sintaxis: INSERT INTO <tabla> VALUES(<valores>)", {}, {}};
    if (toUpper(tokens[1]) != "INTO") return {false, "Se esperaba INTO después de INSERT.", {}, {}};
 
    std::string tableName = tokens[2];
 
    if (!sdm.tableExists(db, tableName)) {
        return {false, "La tabla '" + tableName + "' no existe.", {}, {}};
    }
 
    std::vector<ColumnDefinition> colDefs = sdm.getColumns(db, tableName);
 
    // Buscar '(' puede venir directo o después de VALUES
    int idx = 3;
    if (toUpper(tokens[idx]) == "VALUES") idx++;
    if (tokens[idx] != "(") return {false, "Se esperaba '(' después del nombre de tabla.", {}, {}};
    idx++;
 
    // Recolectar valores hasta ')'
    std::vector<std::string> rawValues;
    while (idx < (int)tokens.size() && tokens[idx] != ")") {
        if (tokens[idx] == ",") { idx++; continue; }
        rawValues.push_back(tokens[idx++]);
    }
 
    if (rawValues.size() != colDefs.size()) {
        return {false, "Número de valores (" + std::to_string(rawValues.size()) +
                       ") no coincide con columnas (" + std::to_string(colDefs.size()) + ").", {}, {}};
    }
 
    // Validar tipos y construir ColumnValues
    std::vector<ColumnValue> values;
    for (size_t i = 0; i < colDefs.size(); ++i) {
        if (!validateValue(rawValues[i], colDefs[i].type)) {
            return {false, "Valor '" + rawValues[i] + "' inválido para columna '" + colDefs[i].name + "'.", {}, {}};
        }
        values.push_back({colDefs[i].type, rawValues[i]});
    }
 
    // Verificar duplicados en columnas indexadas antes de insertar
    for (size_t i = 0; i < colDefs.size(); ++i) {
        if (sdm.hasIndex(db, tableName, colDefs[i].name)) {
            long pos = sdm.lookupIndex(db, tableName, colDefs[i].name, values[i]);
            if (pos >= 0) {
                return {false, "Valor duplicado '" + values[i].raw +
                               "' en columna indexada '" + colDefs[i].name + "'.", {}, {}};
            }
        }
    }
 
    try {
        sdm.insertRecord(db, tableName, values);
    } catch (const std::exception& e) {
        return {false, std::string("Error al insertar: ") + e.what(), {}, {}};
    }
 
    return {true, "Registro insertado correctamente.", {}, {}};
}
 
// SELECT * | col1, col2 FROM <tabla> [WHERE ...] [ORDER BY col ASC|DESC]
 
QueryResult QueryProcessor::handleSelect(const std::vector<std::string>& tokens, const std::string& db) {
    if (db.empty()) return {false, "No hay base de datos activa.", {}, {}};
 
    // Columnas solicitadas (entre SELECT y FROM) 
    std::vector<std::string> requestedCols;
    bool selectAll = false;
    int idx = 1;
 
    while (idx < (int)tokens.size() && toUpper(tokens[idx]) != "FROM") {
        if (tokens[idx] == ",") { idx++; continue; }
        if (tokens[idx] == "*") selectAll = true;
        else requestedCols.push_back(tokens[idx]);
        idx++;
    }
 
    if (idx >= (int)tokens.size()) return {false, "Falta FROM en SELECT.", {}, {}};
    idx++; // saltar FROM
 
    // Nombre de tabla 
    if (idx >= (int)tokens.size()) return {false, "Falta nombre de tabla.", {}, {}};
    std::string tableName = tokens[idx++];
 
    if (!sdm.tableExists(db, tableName)) {
        return {false, "La tabla '" + tableName + "' no existe.", {}, {}};
    }
 
    std::vector<ColumnDefinition> colDefs = sdm.getColumns(db, tableName);
 
    // WHERE opcional
    Filter filter;
    bool hasFilter = false;
 
    if (idx < (int)tokens.size() && toUpper(tokens[idx]) == "WHERE") {
        if (!parseWhere(tokens, idx, filter)) return {false, "Sintaxis de WHERE inválida.", {}, {}};
        hasFilter = true;
 
        // Validar que la columna del WHERE existe en la tabla
        bool colFound = false;
        for (auto& c : colDefs) if (c.name == filter.column) { colFound = true; break; }
        if (!colFound) return {false, "Columna '" + filter.column + "' no existe.", {}, {}};
 
        idx += 4; // WHERE col op val
    }
 
    // ORDER BY opcional 
    std::string orderByCol;
    bool orderAsc = true;
 
    if (idx < (int)tokens.size() && toUpper(tokens[idx]) == "ORDER") {
        idx++;
        if (idx >= (int)tokens.size() || toUpper(tokens[idx]) != "BY") {
            return {false, "Se esperaba BY después de ORDER.", {}, {}};
        }
        idx++;
        if (idx >= (int)tokens.size()) return {false, "Falta columna en ORDER BY.", {}, {}};
        orderByCol = tokens[idx++];
        if (idx < (int)tokens.size()) {
            std::string dir = toUpper(tokens[idx]);
            if (dir == "DESC") { orderAsc = false; idx++; }
            else if (dir == "ASC") idx++;
        }
    }
 
    // Leer registros (índice si aplica, secuencial si no) 
    std::vector<Record> records;
 
    if (hasFilter && filter.op == "=" && sdm.hasIndex(db, tableName, filter.column)) {
        // Búsqueda por índice: solo funciona para '='
        ColumnValue searchVal;
        for (auto& c : colDefs)
            if (c.name == filter.column) { searchVal.type = c.type; break; }
        searchVal.raw = filter.value;
 
        long offset = sdm.lookupIndex(db, tableName, filter.column, searchVal);
        if (offset < 0) {
            records = {}; // no encontrado, tabla vacía
        } else {
            records = sdm.readRecords(db, tableName, &filter);
        }
    } else {
        // Búsqueda secuencial
        records = sdm.readRecords(db, tableName, hasFilter ? &filter : nullptr);
    }
 
    // ORDER BY con Quicksort (std::sort usa introsort basado en quicksort) 
    if (!orderByCol.empty()) {
        int colIdx = -1;
        for (int i = 0; i < (int)colDefs.size(); ++i)
            if (colDefs[i].name == orderByCol) { colIdx = i; break; }
        if (colIdx == -1) return {false, "Columna '" + orderByCol + "' no existe para ORDER BY.", {}, {}};
 
        std::sort(records.begin(), records.end(), [&](const Record& a, const Record& b) {
            const std::string& va = a.values[colIdx].raw;
            const std::string& vb = b.values[colIdx].raw;
            ColumnType t = colDefs[colIdx].type;
            bool less;
            if      (t == ColumnType::INTEGER) less = std::stoi(va) < std::stoi(vb);
            else if (t == ColumnType::DOUBLE)  less = std::stod(va) < std::stod(vb);
            else                               less = va < vb;
            return orderAsc ? less : !less;
        });
    }
 
    // Construir resultado 
    std::vector<int> colIndices;
    if (selectAll) {
        for (int i = 0; i < (int)colDefs.size(); ++i) colIndices.push_back(i);
    } else {
        for (auto& name : requestedCols) {
            bool found = false;
            for (int i = 0; i < (int)colDefs.size(); ++i)
                if (colDefs[i].name == name) { colIndices.push_back(i); found = true; break; }
            if (!found) return {false, "Columna '" + name + "' no existe.", {}, {}};
        }
    }
 
    QueryResult result;
    result.success = true;
    result.message = std::to_string(records.size()) + " fila(s) encontrada(s).";
    for (int ci : colIndices) result.columns.push_back(colDefs[ci].name);
    for (auto& rec : records) {
        std::vector<std::string> row;
        for (int ci : colIndices) row.push_back(columnValueToString(rec.values[ci]));
        result.rows.push_back(row);
    }
 
    return result;
}
 

// UPDATE <tabla> SET <col> = <valor> [WHERE ...]

 
QueryResult QueryProcessor::handleUpdate(const std::vector<std::string>& tokens, const std::string& db) {
    if (db.empty()) return {false, "No hay base de datos activa.", {}, {}};
    if (tokens.size() < 6) return {false, "Sintaxis: UPDATE <tabla> SET <col> = <valor> [WHERE ...]", {}, {}};
 
    std::string tableName = tokens[1];
    if (!sdm.tableExists(db, tableName)) return {false, "La tabla '" + tableName + "' no existe.", {}, {}};
    if (toUpper(tokens[2]) != "SET") return {false, "Se esperaba SET.", {}, {}};
 
    std::string setCol = tokens[3];
    if (tokens[4] != "=") return {false, "Se esperaba '=' en SET.", {}, {}};
    std::string setVal = tokens[5];
 
    // Verificar que la columna existe y obtener su tipo
    std::vector<ColumnDefinition> colDefs = sdm.getColumns(db, tableName);
    ColumnType setType = ColumnType::VARCHAR;
    bool colFound = false;
    for (auto& c : colDefs) {
        if (c.name == setCol) { setType = c.type; colFound = true; break; }
    }
    if (!colFound) return {false, "Columna '" + setCol + "' no existe.", {}, {}};
    if (!validateValue(setVal, setType)) return {false, "Valor inválido para columna '" + setCol + "'.", {}, {}};
 
    ColumnValue newValue = {setType, setVal};
 
    // WHERE opcional
    Filter filter;
    bool hasFilter = false;
    if (tokens.size() > 6 && toUpper(tokens[6]) == "WHERE") {
        if (!parseWhere(tokens, 6, filter)) return {false, "Sintaxis de WHERE inválida.", {}, {}};
        hasFilter = true;
    }
 
    sdm.updateRecords(db, tableName, hasFilter ? &filter : nullptr, setCol, newValue);
    return {true, "Registros actualizados.", {}, {}};
}
 

// DELETE FROM <tabla> [WHERE ...]

QueryResult QueryProcessor::handleDelete(const std::vector<std::string>& tokens, const std::string& db) {
    if (db.empty()) return {false, "No hay base de datos activa.", {}, {}};
    if (tokens.size() < 3) return {false, "Sintaxis: DELETE FROM <tabla> [WHERE ...]", {}, {}};
    if (toUpper(tokens[1]) != "FROM") return {false, "Se esperaba FROM después de DELETE.", {}, {}};
 
    std::string tableName = tokens[2];
    if (!sdm.tableExists(db, tableName)) return {false, "La tabla '" + tableName + "' no existe.", {}, {}};
 
    Filter filter;
    bool hasFilter = false;
    if (tokens.size() > 3 && toUpper(tokens[3]) == "WHERE") {
        if (!parseWhere(tokens, 3, filter)) return {false, "Sintaxis de WHERE inválida.", {}, {}};
        hasFilter = true;
    }
 
    sdm.deleteRecords(db, tableName, hasFilter ? &filter : nullptr);
    return {true, "Registros eliminados.", {}, {}};
}
 

// CREATE INDEX <nombre> ON <tabla>(<col>) OF TYPE <BTREE|BST>

 
QueryResult QueryProcessor::handleCreateIndex(const std::vector<std::string>& tokens, const std::string& db) {
    if (db.empty()) return {false, "No hay base de datos activa.", {}, {}};
    // tokens: CREATE INDEX <nombre> ON <tabla> ( <col> ) OF TYPE <tipo>
    if (tokens.size() < 11) return {false, "Sintaxis: CREATE INDEX <nombre> ON <tabla>(<col>) OF TYPE <BTREE|BST>", {}, {}};
 
    if (toUpper(tokens[3]) != "ON")   return {false, "Se esperaba ON.", {}, {}};
    std::string tableName = tokens[4];
    if (tokens[5] != "(")             return {false, "Se esperaba '('.", {}, {}};
    std::string colName   = tokens[6];
    if (tokens[7] != ")")             return {false, "Se esperaba ')'.", {}, {}};
    if (toUpper(tokens[8]) != "OF")   return {false, "Se esperaba OF.", {}, {}};
    if (toUpper(tokens[9]) != "TYPE") return {false, "Se esperaba TYPE.", {}, {}};
    std::string indexType = toUpper(tokens[10]);
 
    if (indexType != "BTREE" && indexType != "BST") {
        return {false, "Tipo de índice inválido. Use BTREE o BST.", {}, {}};
    }
 
    if (!sdm.tableExists(db, tableName)) return {false, "La tabla '" + tableName + "' no existe.", {}, {}};
 
    // Verificar que la columna existe
    auto colDefs = sdm.getColumns(db, tableName);
    bool colFound = false;
    for (auto& c : colDefs) if (c.name == colName) { colFound = true; break; }
    if (!colFound) return {false, "Columna '" + colName + "' no existe.", {}, {}};
 
    if (sdm.hasIndex(db, tableName, colName)) {
        return {false, "Ya existe un índice en la columna '" + colName + "'.", {}, {}};
    }
 
    try {
        sdm.createIndex(db, tableName, colName, indexType);
    } catch (const std::exception& e) {
        return {false, std::string("Error al crear índice: ") + e.what(), {}, {}};
    }
 
    return {true, "Índice creado en '" + tableName + "." + colName + "'.", {}, {}};
}