#ifndef P3_CE2103_TINYSQLDB_TINYSQLDB_H
#define P3_CE2103_TINYSQLDB_TINYSQLDB_H

#pragma once
#include <string>
#include <vector>

enum class ColumnType {
    INTEGER,
    DOUBLE,
    VARCHAR,
    DATETIME
};

struct ColumnValue {
    ColumnType  type;
    std::string raw;
};

struct ColumnDefinition {
    std::string name;
    ColumnType  type;
    int         maxLength;
    bool        nullable;
};

struct Record {
    std::vector<ColumnValue> values;
    long offset;
};

struct Filter {
    std::string column;
    std::string op;    // "=", ">", "<", "like", "not"
    std::string value;
};


class StoredDataManager {
public:

    // system catalog
    bool databaseExists(const std::string& db);
    bool tableExists(const std::string& db, const std::string& table);
    std::vector<std::string> getDatabases();
    std::vector<std::string> getTables(const std::string& db);
    std::vector<ColumnDefinition> getColumns(const std::string& db, const std::string& table);

    // tablas
    void createDatabase(const std::string& db);
    void createTable(const std::string& db, const std::string& table, const std::vector<ColumnDefinition>& columns);
    void dropTable(const std::string& db, const std::string& table);

    // datos
    long insertRecord(const std::string& db, const std::string& table, const std::vector<ColumnValue>& values);
    std::vector<Record> readRecords(const std::string& db, const std::string& table, const Filter* filter = nullptr);
    void deleteRecords(const std::string& db, const std::string& table, const Filter* filter = nullptr);
    void updateRecords(const std::string& db, const std::string& table, const Filter* filter, const std::string& column, const ColumnValue& newValue);

    // índices
    void createIndex(const std::string& db, const std::string& table, const std::string& column, const std::string& type);
    long lookupIndex(const std::string& db, const std::string& table, const std::string& column, const ColumnValue& value);
    bool hasIndex(const std::string& db, const std::string& table, const std::string& column);
};

class QueryProcessor {
public:

};
#endif //P3_CE2103_TINYSQLDB_TINYSQLDB_H