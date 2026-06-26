#pragma once
#include "Index.h"
#include "../TinySQLDb.h"

// Cada nodo guarda una key (ColumnValue) y el offset del registro en el archivo de la tabla

class BST : public Index {
public:
    BST();
    ~BST() override;

    void insert(const ColumnValue& key, long offset) override;
    long search(const ColumnValue& key) const override;
    void remove(const ColumnValue& key) override;
    bool exists(const ColumnValue& key) const override;

private:
    struct Node {
        ColumnValue key;
        long offset;
        Node* left;
        Node* right;

        Node(const ColumnValue& k, long o) : key(k), offset(o), left(nullptr), right(nullptr) {}
    };

    Node* root;

    // Helpers recursivos (operan sobre un subárbol dado su raíz)
    Node* insertHelper(Node* node, const ColumnValue& key, long offset);
    Node* findHelper(Node* node, const ColumnValue& key) const;
    Node* removeHelper(Node* node, const ColumnValue& key);
    Node* findMin(Node* node) const; // usado por removeHelper para el caso de 2 hijos
    void destroyHelper(Node* node);  // libera toda la memoria del árbol (destructor)
};