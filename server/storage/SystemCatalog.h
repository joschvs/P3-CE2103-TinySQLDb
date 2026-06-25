#pragma once
#include "server/TinySQLDb.h"
#include <map>
#include <string>
#include <vector>
#include "TinySQLDb.h"

class SystemCatalog {
public:
    SystemCatalog(const std::string& rootPath);

    // carga el catalog desde disco a memoria (se llama una vez al arrancar)
    void load();

    // consultas (solo leen memoria)
    bool databaseExists(const std::string& db) const;
    bool tableExists(const std::string& db, const std::string& table) const;
    std::vector<std::string> getDatabases() const;
    std::vector<std::string> getTables(const std::string& db) const;
    std::vector<ColumnDefinition> getColumns(const std::string& db, const std::string& table) const;
    bool hasIndex(const std::string& db, const std::string& table, const std::string& column) const;
    std::string getIndexType(const std::string& db, const std::string& table, const std::string& column) const;

    // modificaciones (actualizan memoria Y disco)
    void addDatabase(const std::string& db);
    void addTable(const std::string& db, const std::string& table, const std::vector<ColumnDefinition>& cols);
    void removeTable(const std::string& db, const std::string& table);
    void addIndex(const std::string& db, const std::string& table, const std::string& column, const std::string& type);

private:
    std::string rootPath; // PROJECT_ROOT + "/data"

    std::vector<std::string> databases;
    std::map<std::string, std::vector<std::string>> tables;
    std::map<std::string, std::vector<ColumnDefinition>> columns;
    std::map<std::string, std::string> indexes;

    // helpers para persistir cada estructura a su archivo binario correspondiente
    void persistDatabases();
    void persistTables();
    void persistColumns();
    void persistIndexes();

    void loadDatabases();
    void loadTables();
    void loadColumns();
    void loadIndexes();

    std::string catalogPath() const; // rootPath + "/system_catalog"
};
