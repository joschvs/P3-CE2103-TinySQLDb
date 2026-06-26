#pragma once
#include "../TinySQLDb.h"
#include "SystemCatalog.h"
#include "Index.h"
#include <string>
#include <vector>
#include <map>

class StoredDataManager {
public:
    StoredDataManager();
    ~StoredDataManager(); // libera la memoria de los índices activos (BST/BTree)

    // System Catalog (delegado a SystemCatalog)
    bool databaseExists(const std::string& db);
    bool tableExists(const std::string& db, const std::string& table);
    std::vector<std::string> getDatabases();
    std::vector<std::string> getTables(const std::string& db);
    std::vector<ColumnDefinition> getColumns(const std::string& db, const std::string& table);

    // Tablas
    void createDatabase(const std::string& db);
    void createTable(const std::string& db, const std::string& table, const std::vector<ColumnDefinition>& columns);
    void dropTable(const std::string& db, const std::string& table);

    // Datos
    long insertRecord(const std::string& db, const std::string& table, const std::vector<ColumnValue>& values);
    std::vector<Record> readRecords(const std::string& db, const std::string& table, const Filter* filter = nullptr);
    void deleteRecords(const std::string& db, const std::string& table, const Filter* filter = nullptr);
    void updateRecords(const std::string& db, const std::string& table, const Filter* filter, const std::string& column, const ColumnValue& newValue);

    // Índices
    void createIndex(const std::string& db, const std::string& table, const std::string& column, const std::string& type);
    long lookupIndex(const std::string& db, const std::string& table, const std::string& column, const ColumnValue& value);
    bool hasIndex(const std::string& db, const std::string& table, const std::string& column);

private:
    std::string rootPath;       // PROJECT_ROOT + "/data"
    SystemCatalog catalog;

    // Índices activos en memoria. Key compuesta "db.tabla.columna"
    std::map<std::string, Index*> indexes;

    // Helpers de rutas a archivos de tabla
    std::string tableFilePath(const std::string& db, const std::string& table) const;

    // Helper de key compuesta para el mapa de índices
    static std::string indexKey(const std::string& db, const std::string& table, const std::string& column);

    // Reconstrucción de índices al arrancar el servidor
    void rebuildIndexesFromCatalog();

    // Helpers de tamaño fijo de registro (necesarios para offsets exactos)
    int columnByteSize(const ColumnDefinition& col) const;
    long recordByteSize(const std::vector<ColumnDefinition>& cols) const;

    // Serialización binaria de un registro
    std::vector<char> serializeRecord(const std::vector<ColumnValue>& values, const std::vector<ColumnDefinition>& cols) const;
    std::vector<ColumnValue> deserializeRecord(const std::vector<char>& bytes, const std::vector<ColumnDefinition>& cols) const;

    // Evaluación de un Filter contra un registro ya leído
    bool matchesFilter(const std::vector<ColumnValue>& values, const std::vector<ColumnDefinition>& cols, const Filter& filter) const;
};