#include "SystemCatalog.h"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;


// Constructor y rutas

SystemCatalog::SystemCatalog(const std::string& rootPath) : rootPath(rootPath) {
    // asegura que la carpeta del catalog exista antes de leer/escribir nada
    fs::create_directories(catalogDir());
}

std::string SystemCatalog::catalogDir() const {
    return rootPath + "/system_catalog";
}

std::string SystemCatalog::databasesFile() const { return catalogDir() + "/SystemDatabases"; }
std::string SystemCatalog::tablesFile()    const { return catalogDir() + "/SystemTables"; }
std::string SystemCatalog::columnsFile()   const { return catalogDir() + "/SystemColumns"; }
std::string SystemCatalog::indexesFile()   const { return catalogDir() + "/SystemIndexes"; }


// Helpers de keys compuestas

std::string SystemCatalog::makeKey(const std::string& db, const std::string& table) {
    return db + "." + table;
}

std::string SystemCatalog::makeKey(const std::string& db, const std::string& table, const std::string& column) {
    return db + "." + table + "." + column;
}


// Helpers internos de lectura/escritura de strings con longitud variable
// Formato: [4 bytes: longitud][N bytes: contenido]

static void writeString(std::ofstream& out, const std::string& s) {
    int len = static_cast<int>(s.size());
    out.write(reinterpret_cast<const char*>(&len), sizeof(int));
    out.write(s.data(), len);
}

static bool readString(std::ifstream& in, std::string& out) {
    int len;
    in.read(reinterpret_cast<char*>(&len), sizeof(int));
    if (in.eof() || in.gcount() < (std::streamsize)sizeof(int)) return false;

    out.resize(len);
    in.read(&out[0], len);
    return true;
}


// LOAD: carga todo el catalog desde disco a memoria (se llama una vez al iniciar)

void SystemCatalog::load() {
    loadDatabases();
    loadTables();
    loadColumns();
    loadIndexes();
}

void SystemCatalog::loadDatabases() {
    databases.clear();
    std::ifstream in(databasesFile(), std::ios::binary);
    if (!in.is_open()) return; // primera vez que corre el servidor, archivo no existe aún

    std::string name;
    while (readString(in, name)) {
        databases.push_back(name);
    }
}

void SystemCatalog::loadTables() {
    tables.clear();
    std::ifstream in(tablesFile(), std::ios::binary);
    if (!in.is_open()) return;

    std::string db, table;
    while (readString(in, db) && readString(in, table)) {
        tables[db].push_back(table);
    }
}

void SystemCatalog::loadColumns() {
    columns.clear();
    std::ifstream in(columnsFile(), std::ios::binary);
    if (!in.is_open()) return;

    std::string db, table, colName;
    while (readString(in, db) && readString(in, table) && readString(in, colName)) {
        ColumnDefinition col;
        col.name = colName;

        int typeAsInt;
        in.read(reinterpret_cast<char*>(&typeAsInt), sizeof(int));
        col.type = static_cast<ColumnType>(typeAsInt);

        in.read(reinterpret_cast<char*>(&col.maxLength), sizeof(int));
        in.read(reinterpret_cast<char*>(&col.nullable), sizeof(bool));

        columns[makeKey(db, table)].push_back(col);
    }
}

void SystemCatalog::loadIndexes() {
    indexes.clear();
    std::ifstream in(indexesFile(), std::ios::binary);
    if (!in.is_open()) return;

    std::string db, table, column, type;
    while (readString(in, db) && readString(in, table) && readString(in, column) && readString(in, type)) {
        indexes[makeKey(db, table, column)] = type;
    }
}


// PERSIST: reescribe el archivo completo desde el caché en memoria
// (no se edita en el lugar, siempre se regenera entero)

void SystemCatalog::persistDatabases() {
    std::ofstream out(databasesFile(), std::ios::binary | std::ios::trunc);
    for (const auto& db : databases) {
        writeString(out, db);
    }
}

void SystemCatalog::persistTables() {
    std::ofstream out(tablesFile(), std::ios::binary | std::ios::trunc);
    for (const auto& [db, tableList] : tables) {
        for (const auto& table : tableList) {
            writeString(out, db);
            writeString(out, table);
        }
    }
}

void SystemCatalog::persistColumns() {
    std::ofstream out(columnsFile(), std::ios::binary | std::ios::trunc);
    for (const auto& [key, colList] : columns) {
        // la key es "db.tabla", la separamos para escribirla en partes
        size_t dotPos = key.find('.');
        std::string db = key.substr(0, dotPos);
        std::string table = key.substr(dotPos + 1);

        for (const auto& col : colList) {
            writeString(out, db);
            writeString(out, table);
            writeString(out, col.name);

            int typeAsInt = static_cast<int>(col.type);
            out.write(reinterpret_cast<const char*>(&typeAsInt), sizeof(int));
            out.write(reinterpret_cast<const char*>(&col.maxLength), sizeof(int));
            out.write(reinterpret_cast<const char*>(&col.nullable), sizeof(bool));
        }
    }
}

void SystemCatalog::persistIndexes() {
    std::ofstream out(indexesFile(), std::ios::binary | std::ios::trunc);
    for (const auto& [key, type] : indexes) {
        // key es "db.tabla.columna", la separamos en sus 3 partes
        size_t firstDot = key.find('.');
        size_t secondDot = key.find('.', firstDot + 1);
        std::string db = key.substr(0, firstDot);
        std::string table = key.substr(firstDot + 1, secondDot - firstDot - 1);
        std::string column = key.substr(secondDot + 1);

        writeString(out, db);
        writeString(out, table);
        writeString(out, column);
        writeString(out, type);
    }
}


// Consultas (solo leen memoria)

bool SystemCatalog::databaseExists(const std::string& db) const {
    for (const auto& d : databases) if (d == db) return true;
    return false;
}

bool SystemCatalog::tableExists(const std::string& db, const std::string& table) const {
    auto it = tables.find(db);
    if (it == tables.end()) return false;
    for (const auto& t : it->second) if (t == table) return true;
    return false;
}

std::vector<std::string> SystemCatalog::getDatabases() const {
    return databases;
}

std::vector<std::string> SystemCatalog::getTables(const std::string& db) const {
    auto it = tables.find(db);
    if (it == tables.end()) return {};
    return it->second;
}

std::vector<ColumnDefinition> SystemCatalog::getColumns(const std::string& db, const std::string& table) const {
    auto it = columns.find(makeKey(db, table));
    if (it == columns.end()) return {};
    return it->second;
}

bool SystemCatalog::hasIndex(const std::string& db, const std::string& table, const std::string& column) const {
    return indexes.find(makeKey(db, table, column)) != indexes.end();
}

std::string SystemCatalog::getIndexType(const std::string& db, const std::string& table, const std::string& column) const {
    auto it = indexes.find(makeKey(db, table, column));
    if (it == indexes.end()) return "";
    return it->second;
}


// Modificaciones (actualizan memoria Y disco)

void SystemCatalog::addDatabase(const std::string& db) {
    databases.push_back(db);
    persistDatabases();

    // crear la carpeta física de la base de datos
    fs::create_directories(rootPath + "/" + db);
}

void SystemCatalog::addTable(const std::string& db, const std::string& table, const std::vector<ColumnDefinition>& cols) {
    tables[db].push_back(table);
    columns[makeKey(db, table)] = cols;

    persistTables();
    persistColumns();
}

void SystemCatalog::removeTable(const std::string& db, const std::string& table) {
    auto& tableList = tables[db];
    tableList.erase(std::remove(tableList.begin(), tableList.end(), table), tableList.end());

    columns.erase(makeKey(db, table));

    persistTables();
    persistColumns();
}

void SystemCatalog::addIndex(const std::string& db, const std::string& table, const std::string& column, const std::string& type) {
    indexes[makeKey(db, table, column)] = type;
    persistIndexes();
}