#include "Index.h"
#include <stdexcept>

// Compara dos ColumnValue según su tipo real.
// Retorna: -1 si a < b, 0 si a == b, 1 si a > b

int Index::compareValues(const ColumnValue& a, const ColumnValue& b) {
    switch (a.type) {
        case ColumnType::INTEGER: {
            int x = std::stoi(a.raw);
            int y = std::stoi(b.raw);
            if (x < y) return -1;
            if (x > y) return 1;
            return 0;
        }
        case ColumnType::DOUBLE: {
            double x = std::stod(a.raw);
            double y = std::stod(b.raw);
            if (x < y) return -1;
            if (x > y) return 1;
            return 0;
        }
        case ColumnType::DATETIME: {
            // compara como strings porque el formato "YYYY-MM-DD HH:MM:SS"
            if (a.raw < b.raw) return -1;
            if (a.raw > b.raw) return 1;
            return 0;
        }
        case ColumnType::VARCHAR: {
            if (a.raw < b.raw) return -1;
            if (a.raw > b.raw) return 1;
            return 0;
        }
    }
    throw std::runtime_error("Tipo de columna desconocido en compareValues");
}