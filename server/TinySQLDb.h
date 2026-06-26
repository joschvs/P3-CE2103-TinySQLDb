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

// QueryResult: estructura que devuelve el QueryProcessor al Web API
struct QueryResult {
    bool success;
    std::string message;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

// StoredDataManager está declarada en server/storage/StoredDataManager.h

#endif //P3_CE2103_TINYSQLDB_TINYSQLDB_H