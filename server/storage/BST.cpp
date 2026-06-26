#include "BST.h"
#include <stdexcept>

BST::BST() : root(nullptr) {}

BST::~BST() {
    destroyHelper(root);
}

void BST::destroyHelper(Node* node) {
    if (node == nullptr) return;
    destroyHelper(node->left);
    destroyHelper(node->right);
    delete node;
}


// insert

void BST::insert(const ColumnValue& key, long offset) {
    if (exists(key)) {
        throw std::runtime_error("No se permiten valores duplicados en una columna indexada");
    }
    root = insertHelper(root, key, offset);
}

BST::Node* BST::insertHelper(Node* node, const ColumnValue& key, long offset) {
    if (node == nullptr) {
        return new Node(key, offset);
    }

    int cmp = compareValues(key, node->key);
    if (cmp < 0) {
        node->left = insertHelper(node->left, key, offset);
    } else if (cmp > 0) {
        node->right = insertHelper(node->right, key, offset);
    }
    // cmp == 0 no debería pasar porque ya validamos exists() antes,
    // pero si pasara, simplemente no hacemos nada (no hay duplicados)

    return node;
}


// search / exists

long BST::search(const ColumnValue& key) const {
    Node* found = findHelper(root, key);
    if (found == nullptr) return -1;
    return found->offset;
}

bool BST::exists(const ColumnValue& key) const {
    return findHelper(root, key) != nullptr;
}

BST::Node* BST::findHelper(Node* node, const ColumnValue& key) const {
    if (node == nullptr) return nullptr;

    int cmp = compareValues(key, node->key);
    if (cmp == 0) return node;       // encontrado
    if (cmp < 0) return findHelper(node->left, key);
    return findHelper(node->right, key);
}


// remove

void BST::remove(const ColumnValue& key) {
    root = removeHelper(root, key);
}

BST::Node* BST::findMin(Node* node) const {
    while (node->left != nullptr) {
        node = node->left;
    }
    return node;
}

BST::Node* BST::removeHelper(Node* node, const ColumnValue& key) {
    if (node == nullptr) return nullptr; // key no encontrada, no hay nada que hacer

    int cmp = compareValues(key, node->key);

    if (cmp < 0) {
        node->left = removeHelper(node->left, key);
    } else if (cmp > 0) {
        node->right = removeHelper(node->right, key);
    } else {
        // Encontramos el nodo a eliminar (cmp == 0)

        // Caso 1: sin hijos, o Caso 2: un solo hijo
        if (node->left == nullptr) {
            Node* temp = node->right;
            delete node;
            return temp; // si right también es nullptr, esto retorna nullptr (Caso 1)
        }
        if (node->right == nullptr) {
            Node* temp = node->left;
            delete node;
            return temp;
        }

        // Caso 3: dos hijos -> reemplazar con el mínimo del subárbol derecho
        Node* successor = findMin(node->right);
        node->key = successor->key;
        node->offset = successor->offset;

        // Ahora eliminamos el sucesor de su posición original (en el subárbol derecho)
        node->right = removeHelper(node->right, successor->key);
    }

    return node;
}