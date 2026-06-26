#pragma once
#include "Index.h"
#include "../TinySQLDb.h"
#include <vector>

// grado mínimo t = 3.
// Cada nodo (excepto la raíz) tiene entre t-1=2 y 2t-1=5 keys.
// Cada nodo interno tiene entre t=3 y 2t=6 hijos.
// Las keys dentro de cada nodo están siempre ordenadas.

class BTree : public Index {
public:
    BTree();
    ~BTree() override;

    void insert(const ColumnValue& key, long offset) override;
    long search(const ColumnValue& key) const override;
    void remove(const ColumnValue& key) override;
    bool exists(const ColumnValue& key) const override;

private:
    static const int T = 3; // grado mínimo

    struct Node {
        std::vector<ColumnValue> keys;
        std::vector<long> offsets;       // paralelo a keys: offsets[i] es el offset de keys[i]
        std::vector<Node*> children;     // vacío si isLeaf == true
        bool isLeaf;

        Node(bool leaf) : isLeaf(leaf) {}
    };

    Node* root;

    // Helpers de búsqueda
    long searchHelper(Node* node, const ColumnValue& key) const;

    // Helpers de inserción
    void insertNonFull(Node* node, const ColumnValue& key, long offset);
    void splitChild(Node* parent, int childIndex);

    // Helpers de eliminación
    void removeHelper(Node* node, const ColumnValue& key);
    void removeFromLeaf(Node* node, int idx);
    void removeFromInternal(Node* node, int idx);
    void fillChild(Node* node, int childIndex); // asegura que el hijo tenga >= t keys antes de bajar
    void borrowFromPrev(Node* node, int childIndex);
    void borrowFromNext(Node* node, int childIndex);
    void mergeChildren(Node* node, int childIndex);
    int findKeyIndex(Node* node, const ColumnValue& key) const; // posición de la key en node, o node->keys.size() si no está
    Node* getPredecessorNode(Node* node) const; // nodo hoja con el máximo del subárbol
    Node* getSuccessorNode(Node* node) const;   // nodo hoja con el mínimo del subárbol

    // Helper de liberación de memoria (destructor)
    void destroyHelper(Node* node);
};