#pragma once
#include "../TinySQLDb.h"
#include <vector>

// Index: clase base abstracta para los índices (BTREE y BST)
// La key es siempre un ColumnValue completo (no un tipo nativo de C++), para evitar duplicar la lógica de comparación en cada implementación
// compareValues() es estático y compartido por BTree y BST: dado que ambos indexan ColumnValue, ambos necesitan la misma lógica de comparación

class Index {
public:
    virtual ~Index() = default;

    // Inserta una key (valor de columna) asociada a un offset en el archivo de la tabla.
    // Si la key ya existe, debe lanzar std::runtime_error (no se permiten duplicados).
    virtual void insert(const ColumnValue& key, long offset) = 0;

    // Busca una key y retorna su offset. Retorna -1 si no existe.
    virtual long search(const ColumnValue& key) const = 0;

    // Elimina una key del índice (no toca el archivo de la tabla, solo el índice en memoria).
    virtual void remove(const ColumnValue& key) = 0;

    // Indica si una key ya existe en el índice.
    virtual bool exists(const ColumnValue& key) const = 0;

protected:
    // Compara dos ColumnValue según su tipo real.
    // Retorna: -1 si a < b, 0 si a == b, 1 si a > b
    static int compareValues(const ColumnValue& a, const ColumnValue& b);
};
