#include "BTree.h"
#include <stdexcept>

BTree::BTree() {
    root = new Node(true); // la raíz arranca como hoja vacía
}

BTree::~BTree() {
    destroyHelper(root);
}

void BTree::destroyHelper(Node* node) {
    if (node == nullptr) return;
    if (!node->isLeaf) {
        for (Node* child : node->children) {
            destroyHelper(child);
        }
    }
    delete node;
}


// splitChild: divide el hijo lleno parent->children[childIndex] en dos, y sube la key del medio al parent


void BTree::splitChild(Node* parent, int childIndex) {
    Node* fullChild = parent->children[childIndex];
    Node* newChild = new Node(fullChild->isLeaf);

    // La key del medio (posición t-1) es la que sube al padre
    ColumnValue midKey = fullChild->keys[T - 1];
    long midOffset = fullChild->offsets[T - 1];

    // newChild se lleva las keys/offsets de la derecha de la del medio (posiciones T..2T-2)
    for (int i = T; i < 2 * T - 1; ++i) {
        newChild->keys.push_back(fullChild->keys[i]);
        newChild->offsets.push_back(fullChild->offsets[i]);
    }

    // Si no es hoja, también se reparten los hijos: los últimos T se van a newChild
    if (!fullChild->isLeaf) {
        for (int i = T; i < 2 * T; ++i) {
            newChild->children.push_back(fullChild->children[i]);
        }
    }

    // fullChild se queda solo con las keys/offsets de la izquierda (posiciones 0..T-2)
    // y, si aplica, los primeros T hijos (0..T-1)
    fullChild->keys.resize(T - 1);
    fullChild->offsets.resize(T - 1);
    if (!fullChild->isLeaf) {
        fullChild->children.resize(T);
    }

    // Insertamos newChild como hijo del parent, justo después de fullChild
    parent->children.insert(parent->children.begin() + childIndex + 1, newChild);

    // Insertamos la key del medio en el parent, en la posición correspondiente
    parent->keys.insert(parent->keys.begin() + childIndex, midKey);
    parent->offsets.insert(parent->offsets.begin() + childIndex, midOffset);
}


// insert

void BTree::insert(const ColumnValue& key, long offset) {
    if (exists(key)) {
        throw std::runtime_error("No se permiten valores duplicados en una columna indexada");
    }

    // Caso especial: si la raíz ya está llena, hay que dividirla ANTES de insertar.
    // Esto hace crecer el árbol en altura (la única forma en que un BTree crece hacia arriba).
    if (static_cast<int>(root->keys.size()) == 2 * T - 1) {
        Node* newRoot = new Node(false); // la nueva raíz nunca es hoja
        newRoot->children.push_back(root);
        splitChild(newRoot, 0);
        root = newRoot;
    }

    insertNonFull(root, key, offset);
}

void BTree::insertNonFull(Node* node, const ColumnValue& key, long offset) {
    // Buscamos la posición donde debería ir la key dentro de este nodo
    int i = static_cast<int>(node->keys.size()) - 1;

    if (node->isLeaf) {
        // Insertamos directamente en la posición correcta, desplazando lo necesario
        node->keys.push_back(key);     // espacio temporal al final
        node->offsets.push_back(offset);

        while (i >= 0 && compareValues(key, node->keys[i]) < 0) {
            node->keys[i + 1] = node->keys[i];
            node->offsets[i + 1] = node->offsets[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->offsets[i + 1] = offset;

    } else {
        // No es hoja: buscamos a cuál hijo debemos bajar
        while (i >= 0 && compareValues(key, node->keys[i]) < 0) {
            i--;
        }
        i++; // i queda en el índice del hijo correcto

        // Si ese hijo está lleno, lo dividimos ANTES de bajar (estrategia preventiva)
        if (static_cast<int>(node->children[i]->keys.size()) == 2 * T - 1) {
            splitChild(node, i);
            // Después del split, puede que la nueva key del medio en 'node'
            // determine que ahora debemos ir al hijo de la derecha en vez del original
            if (compareValues(key, node->keys[i]) > 0) {
                i++;
            }
        }

        insertNonFull(node->children[i], key, offset);
    }
}


// search / exists

long BTree::search(const ColumnValue& key) const {
    return searchHelper(root, key);
}

bool BTree::exists(const ColumnValue& key) const {
    return searchHelper(root, key) != -1;
}

long BTree::searchHelper(Node* node, const ColumnValue& key) const {
    int i = 0;
    while (i < static_cast<int>(node->keys.size()) && compareValues(key, node->keys[i]) > 0) {
        i++;
    }

    // Si encontramos la key exacta en este nodo, retornamos su offset
    if (i < static_cast<int>(node->keys.size()) && compareValues(key, node->keys[i]) == 0) {
        return node->offsets[i];
    }

    // Si es hoja y no la encontramos aquí, no existe en el árbol
    if (node->isLeaf) {
        return -1;
    }

    // Si no es hoja, seguimos buscando en el hijo correspondiente
    return searchHelper(node->children[i], key);
}


// remove: helpers de localización

// Retorna el índice de 'key' dentro de node->keys, o node->keys.size() si no está
int BTree::findKeyIndex(Node* node, const ColumnValue& key) const {
    int idx = 0;
    while (idx < static_cast<int>(node->keys.size()) && compareValues(key, node->keys[idx]) > 0) {
        idx++;
    }
    return idx;
}

// Desciende siempre por el último hijo hasta llegar a una hoja (el máximo del subárbol)
BTree::Node* BTree::getPredecessorNode(Node* node) const {
    while (!node->isLeaf) {
        node = node->children.back();
    }
    return node;
}

// Desciende siempre por el primer hijo hasta llegar a una hoja (el mínimo del subárbol)
BTree::Node* BTree::getSuccessorNode(Node* node) const {
    while (!node->isLeaf) {
        node = node->children.front();
    }
    return node;
}


// Caso A: la key está en un nodo hoja -> simplemente la quitamos

void BTree::removeFromLeaf(Node* node, int idx) {
    node->keys.erase(node->keys.begin() + idx);
    node->offsets.erase(node->offsets.begin() + idx);
}


// Caso B: la key está en un nodo interno (no hoja)

void BTree::removeFromInternal(Node* node, int idx) {
    ColumnValue key = node->keys[idx];
    Node* leftChild = node->children[idx];
    Node* rightChild = node->children[idx + 1];

    if (static_cast<int>(leftChild->keys.size()) >= T) {
        // B1: el hijo izquierdo tiene keys de sobra -> usamos el predecesor
        Node* predNode = getPredecessorNode(leftChild);
        ColumnValue predKey = predNode->keys.back();
        long predOffset = predNode->offsets.back();

        node->keys[idx] = predKey;
        node->offsets[idx] = predOffset;

        removeHelper(leftChild, predKey); // eliminamos el predecesor de su posición original

    } else if (static_cast<int>(rightChild->keys.size()) >= T) {
        // B2: el hijo derecho tiene keys de sobra -> usamos el sucesor
        Node* succNode = getSuccessorNode(rightChild);
        ColumnValue succKey = succNode->keys.front();
        long succOffset = succNode->offsets.front();

        node->keys[idx] = succKey;
        node->offsets[idx] = succOffset;

        removeHelper(rightChild, succKey);

    } else {
        // B3: ambos hijos tienen el mínimo de keys -> los fusionamos
        mergeChildren(node, idx);
        removeHelper(leftChild, key); // ahora leftChild contiene todo lo fusionado
    }
}


// mergeChildren: fusiona node->children[childIndex] con node->children[childIndex+1],
// bajando la key intermedia node->keys[childIndex] al medio de la fusión.
// Se usa tanto en el Caso B3 (eliminar key interna) como en el Caso C (rebalanceo).

void BTree::mergeChildren(Node* node, int childIndex) {
    Node* leftChild = node->children[childIndex];
    Node* rightChild = node->children[childIndex + 1];

    // La key intermedia baja al final de leftChild
    leftChild->keys.push_back(node->keys[childIndex]);
    leftChild->offsets.push_back(node->offsets[childIndex]);

    // Todas las keys/offsets de rightChild se agregan después
    for (size_t i = 0; i < rightChild->keys.size(); ++i) {
        leftChild->keys.push_back(rightChild->keys[i]);
        leftChild->offsets.push_back(rightChild->offsets[i]);
    }

    // Si no son hojas, los hijos de rightChild también se agregan a leftChild
    if (!leftChild->isLeaf) {
        for (Node* child : rightChild->children) {
            leftChild->children.push_back(child);
        }
    }

    // Quitamos la key intermedia y el puntero a rightChild del nodo padre
    node->keys.erase(node->keys.begin() + childIndex);
    node->offsets.erase(node->offsets.begin() + childIndex);
    node->children.erase(node->children.begin() + childIndex + 1);

    delete rightChild; // ya copiamos todo su contenido a leftChild, liberamos su memoria
}


// borrowFromPrev: el hijo en childIndex está corto de keys, pero su hermano IZQUIERDO (childIndex-1) tiene de sobra.
// se le presta la última key del hermano, pasando por el padre (rotación).

void BTree::borrowFromPrev(Node* node, int childIndex) {
    Node* child = node->children[childIndex];
    Node* leftSibling = node->children[childIndex - 1];

    // La key del padre que separaba a ambos hermanos baja como primera key de 'child'
    child->keys.insert(child->keys.begin(), node->keys[childIndex - 1]);
    child->offsets.insert(child->offsets.begin(), node->offsets[childIndex - 1]);

    // Si no son hojas, el último hijo del hermano izquierdo pasa a ser el primer hijo de 'child'
    if (!child->isLeaf) {
        child->children.insert(child->children.begin(), leftSibling->children.back());
        leftSibling->children.pop_back();
    }

    // La última key del hermano izquierdo sube a ocupar el lugar en el padre
    node->keys[childIndex - 1] = leftSibling->keys.back();
    node->offsets[childIndex - 1] = leftSibling->offsets.back();

    leftSibling->keys.pop_back();
    leftSibling->offsets.pop_back();
}


// borrowFromNext: análogo a borrowFromPrev, pero el hermano con keys de sobra está a la DERECHA (childIndex+1).

void BTree::borrowFromNext(Node* node, int childIndex) {
    Node* child = node->children[childIndex];
    Node* rightSibling = node->children[childIndex + 1];

    // La key del padre que separaba a ambos hermanos se agrega como última key de 'child'
    child->keys.push_back(node->keys[childIndex]);
    child->offsets.push_back(node->offsets[childIndex]);

    // Si no son hojas, el primer hijo del hermano derecho pasa a ser el último hijo de 'child'
    if (!child->isLeaf) {
        child->children.push_back(rightSibling->children.front());
        rightSibling->children.erase(rightSibling->children.begin());
    }

    // La primera key del hermano derecho sube a ocupar el lugar en el padre
    node->keys[childIndex] = rightSibling->keys.front();
    node->offsets[childIndex] = rightSibling->offsets.front();

    rightSibling->keys.erase(rightSibling->keys.begin());
    rightSibling->offsets.erase(rightSibling->offsets.begin());
}


// fillChild: asegura que node->children[childIndex] tenga al menos T keys,

void BTree::fillChild(Node* node, int childIndex) {
    // Hay hermano izquierdo con keys de sobra (más de T-1)?
    if (childIndex > 0 && static_cast<int>(node->children[childIndex - 1]->keys.size()) >= T) {
        borrowFromPrev(node, childIndex);
        return;
    }

    // Hay hermano derecho con keys de sobra?
    if (childIndex < static_cast<int>(node->children.size()) - 1 &&
        static_cast<int>(node->children[childIndex + 1]->keys.size()) >= T) {
        borrowFromNext(node, childIndex);
        return;
    }

    // Ningún hermano tiene de sobra -> fusionar con uno de ellos
    if (childIndex < static_cast<int>(node->children.size()) - 1) {
        mergeChildren(node, childIndex); // fusiona con el hermano derecho
    } else {
        mergeChildren(node, childIndex - 1); // fusiona con el hermano izquierdo
    }
}


// removeHelper: orquesta los 3 casos (A, B, C) de forma recursiva

void BTree::removeHelper(Node* node, const ColumnValue& key) {
    int idx = findKeyIndex(node, key);

    //  key está en este nodo?
    if (idx < static_cast<int>(node->keys.size()) && compareValues(key, node->keys[idx]) == 0) {
        if (node->isLeaf) {
            removeFromLeaf(node, idx);       // Caso A
        } else {
            removeFromInternal(node, idx);   // Caso B
        }
    } else {
        // La key no está aquí; hay que bajar a un hijo
        if (node->isLeaf) {
            return; // la key no existe en el árbol, no hay nada que hacer
        }

        bool isLastChild = (idx == static_cast<int>(node->keys.size()));

        // Caso C: si el hijo al que vamos a bajar tiene el mínimo de keys, lo "engordamos" primero
        if (static_cast<int>(node->children[idx]->keys.size()) < T) {
            fillChild(node, idx);
        }

        if (isLastChild && idx > static_cast<int>(node->keys.size())) {
            removeHelper(node->children[idx - 1], key);
        } else {
            removeHelper(node->children[idx], key);
        }
    }
}


// remove: punto de entrada público

void BTree::remove(const ColumnValue& key) {
    removeHelper(root, key);

    // Si la raíz quedó vacía (sin keys) y no es hoja, su único hijo pasa a ser la nueva raíz
    if (root->keys.empty() && !root->isLeaf) {
        Node* oldRoot = root;
        root = root->children[0];
        delete oldRoot;
    }
}