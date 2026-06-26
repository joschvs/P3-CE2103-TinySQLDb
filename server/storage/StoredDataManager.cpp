#include "StoredDataManager.h"
#include "BST.h"
#include "BTree.h"
#include <fstream>
#include <filesystem>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;


// Constructor / Destructor

StoredDataManager::StoredDataManager()
    : rootPath(std::string(PROJECT_ROOT) + "/data"), catalog(rootPath) {
    fs::create_directories(rootPath);
    catalog.load(); // carga el system catalog completo a memoria al arrancar
    rebuildIndexesFromCatalog(); // reconstruye los árboles en memoria leyendo las tablas
}

StoredDataManager::~StoredDataManager() {
    for (auto& [key, index] : indexes) {
        delete index;
    }
}

std::string StoredDataManager::indexKey(const std::string& db, const std::string& table, const std::string& column) {
    return db + "." + table + "." + column;
}


// Helpers de rutas y tamaños

std::string StoredDataManager::tableFilePath(const std::string& db, const std::string& table) const {
    return rootPath + "/" + db + "/" + table + ".bin";
}

int StoredDataManager::columnByteSize(const ColumnDefinition& col) const {
    switch (col.type) {
        case ColumnType::INTEGER:  return 4;
        case ColumnType::DOUBLE:   return 8;
        case ColumnType::DATETIME: return 8;
        case ColumnType::VARCHAR:  return col.maxLength;
    }
    return 0;
}

// Cada registro tiene 1 byte extra al inicio: flag de eliminado (1 = activo, 0 = eliminado)
long StoredDataManager::recordByteSize(const std::vector<ColumnDefinition>& cols) const {
    long total = 1; // byte de flag
    for (const auto& col : cols) {
        total += columnByteSize(col);
    }
    return total;
}


// Helpers de fecha: conversión entre string "YYYY-MM-DD HH:MM:SS" y timestamp Unix

static long dateTimeStringToTimestamp(const std::string& s) {
    struct tm tmStruct = {};
    int year, month, day, hour, minute, second;
    std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    tmStruct.tm_year = year - 1900; // tm_year es años desde 1900
    tmStruct.tm_mon  = month - 1;   // tm_mon es 0-11, no 1-12
    tmStruct.tm_mday = day;
    tmStruct.tm_hour = hour;
    tmStruct.tm_min  = minute;
    tmStruct.tm_sec  = second;

    return static_cast<long>(mktime(&tmStruct));
}

static std::string timestampToDateTimeString(long timestamp) {
    time_t t = static_cast<time_t>(timestamp);
    struct tm* tmStruct = localtime(&t);
    char buffer[20]; // "YYYY-MM-DD HH:MM:SS" = 19 caracteres + '\0'
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tmStruct);
    return std::string(buffer);
}


// Serialización binaria de un registro (tamaño fijo por columna)

std::vector<char> StoredDataManager::serializeRecord(const std::vector<ColumnValue>& values, const std::vector<ColumnDefinition>& cols) const {
    long totalSize = recordByteSize(cols);
    std::vector<char> buffer(totalSize, '\0'); // inicializado en 0 para que el padding sea correcto

    buffer[0] = 1; // flag de eliminado: 1 = activo (todo registro nuevo nace activo)
    long pos = 1;  // arrancamos en 1 porque el byte 0 ya es el flag

    for (size_t i = 0; i < cols.size(); ++i) {
        const ColumnDefinition& col = cols[i];
        const std::string& raw = values[i].raw;
        int size = columnByteSize(col);

        switch (col.type) {
            case ColumnType::INTEGER: {
                int val = std::stoi(raw);
                std::memcpy(buffer.data() + pos, &val, sizeof(int));
                break;
            }
            case ColumnType::DOUBLE: {
                double val = std::stod(raw);
                std::memcpy(buffer.data() + pos, &val, sizeof(double));
                break;
            }
            case ColumnType::DATETIME: {
                long val = dateTimeStringToTimestamp(raw);
                std::memcpy(buffer.data() + pos, &val, sizeof(long));
                break;
            }
            case ColumnType::VARCHAR: {
                // copiamos como máximo 'size' bytes; el resto queda en '\0' (ya inicializado)
                size_t len = std::min(raw.size(), static_cast<size_t>(size));
                std::memcpy(buffer.data() + pos, raw.data(), len);
                break;
            }
        }
        pos += size;
    }

    return buffer;
}

std::vector<ColumnValue> StoredDataManager::deserializeRecord(const std::vector<char>& bytes, const std::vector<ColumnDefinition>& cols) const {
    std::vector<ColumnValue> values;
    long pos = 1; // el byte 0 es el flag de eliminado, los datos arrancan en 1

    for (const auto& col : cols) {
        int size = columnByteSize(col);
        ColumnValue cv;
        cv.type = col.type;

        switch (col.type) {
            case ColumnType::INTEGER: {
                int val;
                std::memcpy(&val, bytes.data() + pos, sizeof(int));
                cv.raw = std::to_string(val);
                break;
            }
            case ColumnType::DOUBLE: {
                double val;
                std::memcpy(&val, bytes.data() + pos, sizeof(double));
                cv.raw = std::to_string(val);
                break;
            }
            case ColumnType::DATETIME: {
                long val;
                std::memcpy(&val, bytes.data() + pos, sizeof(long));
                cv.raw = timestampToDateTimeString(val);
                break;
            }
            case ColumnType::VARCHAR: {
                // cortar en el primer '\0' para recuperar el string real (sin el padding)
                const char* start = bytes.data() + pos;
                size_t len = 0;
                while (len < static_cast<size_t>(size) && start[len] != '\0') len++;
                cv.raw = std::string(start, len);
                break;
            }
        }
        values.push_back(cv);
        pos += size;
    }

    return values;
}


// Evaluación de un Filter contra un registro ya leído

bool StoredDataManager::matchesFilter(const std::vector<ColumnValue>& values, const std::vector<ColumnDefinition>& cols, const Filter& filter) const {
    // Encontrar el índice de la columna mencionada en el filtro
    int colIndex = -1;
    for (size_t i = 0; i < cols.size(); ++i) {
        if (cols[i].name == filter.column) {
            colIndex = static_cast<int>(i);
            break;
        }
    }
    if (colIndex == -1) return false; // columna no encontrada, no debería pasar si Persona 2 ya validó

    const ColumnValue& recordValue = values[colIndex];
    const std::string& filterValue = filter.value;

    // Comparar según el tipo real de la columna
    switch (recordValue.type) {
        case ColumnType::INTEGER: {
            int a = std::stoi(recordValue.raw);
            int b = std::stoi(filterValue);
            if (filter.op == "=")  return a == b;
            if (filter.op == ">")  return a > b;
            if (filter.op == "<")  return a < b;
            if (filter.op == "not") return a != b;
            break;
        }
        case ColumnType::DOUBLE: {
            double a = std::stod(recordValue.raw);
            double b = std::stod(filterValue);
            if (filter.op == "=")  return a == b;
            if (filter.op == ">")  return a > b;
            if (filter.op == "<")  return a < b;
            if (filter.op == "not") return a != b;
            break;
        }
        case ColumnType::DATETIME: {
            long a = dateTimeStringToTimestamp(recordValue.raw);
            long b = dateTimeStringToTimestamp(filterValue);
            if (filter.op == "=")  return a == b;
            if (filter.op == ">")  return a > b;
            if (filter.op == "<")  return a < b;
            if (filter.op == "not") return a != b;
            break;
        }
        case ColumnType::VARCHAR: {
            const std::string& a = recordValue.raw;
            const std::string& b = filterValue;
            if (filter.op == "=")    return a == b;
            if (filter.op == "not")  return a != b;
            if (filter.op == "like") return a.find(b) != std::string::npos;
            break;
        }
    }
    return false;
}


// System Catalog: delegación directa a SystemCatalog

bool StoredDataManager::databaseExists(const std::string& db) {
    return catalog.databaseExists(db);
}

bool StoredDataManager::tableExists(const std::string& db, const std::string& table) {
    return catalog.tableExists(db, table);
}

std::vector<std::string> StoredDataManager::getDatabases() {
    return catalog.getDatabases();
}

std::vector<std::string> StoredDataManager::getTables(const std::string& db) {
    return catalog.getTables(db);
}

std::vector<ColumnDefinition> StoredDataManager::getColumns(const std::string& db, const std::string& table) {
    return catalog.getColumns(db, table);
}

bool StoredDataManager::hasIndex(const std::string& db, const std::string& table, const std::string& column) {
    return catalog.hasIndex(db, table, column);
}


// Tablas

void StoredDataManager::createDatabase(const std::string& db) {
    catalog.addDatabase(db);
}

void StoredDataManager::createTable(const std::string& db, const std::string& table, const std::vector<ColumnDefinition>& columns) {
    catalog.addTable(db, table, columns);

    // Crear el archivo binario vacío de la tabla
    std::ofstream file(tableFilePath(db, table), std::ios::binary);
    // Se crea vacío; los registros se agregan con insertRecord
}

void StoredDataManager::dropTable(const std::string& db, const std::string& table) {
    catalog.removeTable(db, table);

    // Borrar el archivo físico de la tabla
    fs::remove(tableFilePath(db, table));
}


// Datos: insertRecord

long StoredDataManager::insertRecord(const std::string& db, const std::string& table, const std::vector<ColumnValue>& values) {
    std::vector<ColumnDefinition> cols = catalog.getColumns(db, table);
    std::string path = tableFilePath(db, table);

    // El offset donde va a quedar el nuevo registro es el tamaño actual del archivo
    long offset = fs::exists(path) ? static_cast<long>(fs::file_size(path)) : 0;

    std::vector<char> bytes = serializeRecord(values, cols);

    // Abrimos en modo append (al final) para no sobreescribir lo existente
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        return -1;
    }
    file.write(bytes.data(), bytes.size());
    file.close();

    // Sincronizar con cualquier índice existente sobre alguna de estas columnas
    for (size_t i = 0; i < cols.size(); ++i) {
        std::string key = indexKey(db, table, cols[i].name);
        auto it = indexes.find(key);
        if (it != indexes.end()) {
            it->second->insert(values[i], offset);
        }
    }

    return offset;
}


// Datos: readRecords

std::vector<Record> StoredDataManager::readRecords(const std::string& db, const std::string& table, const Filter* filter) {
    std::vector<Record> results;
    std::vector<ColumnDefinition> cols = catalog.getColumns(db, table);
    std::string path = tableFilePath(db, table);

    if (!fs::exists(path)) return results;

    long recSize = recordByteSize(cols);
    if (recSize == 0) return results;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return results;

    long offset = 0;
    std::vector<char> buffer(recSize);

    while (file.read(buffer.data(), recSize)) {
        bool isActive = buffer[0] == 1;

        if (isActive) {
            std::vector<ColumnValue> values = deserializeRecord(buffer, cols);

            if (filter == nullptr || matchesFilter(values, cols, *filter)) {
                Record r;
                r.values = values;
                r.offset = offset;
                results.push_back(r);
            }
        }

        offset += recSize;
    }

    return results;
}


// Datos: deleteRecords (búsqueda secuencial, sin índice todavía)

void StoredDataManager::deleteRecords(const std::string& db, const std::string& table, const Filter* filter) {
    std::string path = tableFilePath(db, table);
    if (!fs::exists(path)) return;

    std::vector<ColumnDefinition> cols = catalog.getColumns(db, table);

    std::vector<Record> matches = readRecords(db, table, filter);

    // Abrimos en modo lectura+escritura para poder hacer seek() y sobreescribir solo el flag
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return;

    char inactiveFlag = 0;
    for (const auto& record : matches) {
        file.seekp(record.offset); // ir directo al offset de ese registro
        file.write(&inactiveFlag, 1); // sobreescribir solo el primer byte (el flag)

        // Sincronizar con cualquier índice existente: quitar la entrada de este registro
        for (size_t i = 0; i < cols.size(); ++i) {
            std::string key = indexKey(db, table, cols[i].name);
            auto it = indexes.find(key);
            if (it != indexes.end()) {
                it->second->remove(record.values[i]);
            }
        }
    }
}


// Datos: updateRecords (búsqueda secuencial, sin índice todavía)

void StoredDataManager::updateRecords(const std::string& db, const std::string& table, const Filter* filter, const std::string& column, const ColumnValue& newValue) {
    std::string path = tableFilePath(db, table);
    if (!fs::exists(path)) return;

    std::vector<ColumnDefinition> cols = catalog.getColumns(db, table);

    // Encontrar en qué posición (índice) y en qué byte offset dentro del registro está la columna que vamos a actualizar
    int colIndex = -1;
    long byteOffsetInRecord = 1; // arranca en 1 porque el byte 0 es el flag de eliminado
    for (size_t i = 0; i < cols.size(); ++i) {
        if (cols[i].name == column) {
            colIndex = static_cast<int>(i);
            break;
        }
        byteOffsetInRecord += columnByteSize(cols[i]);
    }
    if (colIndex == -1) return; // columna no encontrada

    int colSize = columnByteSize(cols[colIndex]);

    // Encontrar qué registros cumplen el filtro (igual que en deleteRecords)
    std::vector<Record> matches = readRecords(db, table, filter);

    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return;

    // Serializamos solo el nuevo valor de esa columna a sus bytes fijos
    std::vector<ColumnValue> singleValue = {newValue};
    std::vector<ColumnDefinition> singleCol = {cols[colIndex]};
    std::vector<char> newBytes(colSize, '\0');

    switch (cols[colIndex].type) {
        case ColumnType::INTEGER: {
            int val = std::stoi(newValue.raw);
            std::memcpy(newBytes.data(), &val, sizeof(int));
            break;
        }
        case ColumnType::DOUBLE: {
            double val = std::stod(newValue.raw);
            std::memcpy(newBytes.data(), &val, sizeof(double));
            break;
        }
        case ColumnType::DATETIME: {
            long val = dateTimeStringToTimestamp(newValue.raw);
            std::memcpy(newBytes.data(), &val, sizeof(long));
            break;
        }
        case ColumnType::VARCHAR: {
            size_t len = std::min(newValue.raw.size(), static_cast<size_t>(colSize));
            std::memcpy(newBytes.data(), newValue.raw.data(), len);
            break;
        }
    }

    // Sincronizar con el índice de ESTA columna (si existe)
    std::string key = indexKey(db, table, column);
    auto idxIt = indexes.find(key);

    for (const auto& record : matches) {
        file.seekp(record.offset + byteOffsetInRecord); // ir directo a la columna dentro del registro
        file.write(newBytes.data(), colSize);

        if (idxIt != indexes.end()) {
            idxIt->second->remove(record.values[colIndex]); // valor viejo
            idxIt->second->insert(newValue, record.offset);  // valor nuevo
        }
    }
}


// Índices: createIndex

void StoredDataManager::createIndex(const std::string& db, const std::string& table, const std::string& column, const std::string& type) {
    std::string key = indexKey(db, table, column);

    // Creamos el árbol correcto según el tipo pedido
    Index* index;
    if (type == "BTREE") {
        index = new BTree();
    } else { // "BST"
        index = new BST();
    }

    // Encontrar la posición de la columna dentro de las definiciones, para poder extraer el ColumnValue correcto de cada registro existente
    std::vector<ColumnDefinition> cols = catalog.getColumns(db, table);
    int colIndex = -1;
    for (size_t i = 0; i < cols.size(); ++i) {
        if (cols[i].name == column) {
            colIndex = static_cast<int>(i);
            break;
        }
    }

    // Se leen los registros existentes y se insertan en el árbol nuevo
    if (colIndex != -1) {
        std::vector<Record> allRecords = readRecords(db, table, nullptr);
        for (const auto& record : allRecords) {
            index->insert(record.values[colIndex], record.offset);
        }
    }

    indexes[key] = index;
    catalog.addIndex(db, table, column, type); // persistir en el catalog que este índice existe
}


// Índices: lookupIndex

long StoredDataManager::lookupIndex(const std::string& db, const std::string& table, const std::string& column, const ColumnValue& value) {
    std::string key = indexKey(db, table, column);
    auto it = indexes.find(key);
    if (it == indexes.end()) return -1; // no hay índice en esta columna

    return it->second->search(value);
}


// Índices: reconstrucción al arrancar el servidor

void StoredDataManager::rebuildIndexesFromCatalog() {
    // se recorren todas las bases de datos y tablas para encontrar qué índices existían según el catalog, y los reconstruimos leyendo cada tabla
    for (const auto& db : catalog.getDatabases()) {
        for (const auto& table : catalog.getTables(db)) {
            std::vector<ColumnDefinition> cols = catalog.getColumns(db, table);

            for (size_t i = 0; i < cols.size(); ++i) {
                const std::string& column = cols[i].name;

                if (!catalog.hasIndex(db, table, column)) continue;

                std::string type = catalog.getIndexType(db, table, column);
                Index* index = (type == "BTREE") ? static_cast<Index*>(new BTree())
                                                   : static_cast<Index*>(new BST());

                std::vector<Record> allRecords = readRecords(db, table, nullptr);
                for (const auto& record : allRecords) {
                    index->insert(record.values[i], record.offset);
                }

                indexes[indexKey(db, table, column)] = index;
            }
        }
    }
}