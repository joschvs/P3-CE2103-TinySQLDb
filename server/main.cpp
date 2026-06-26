#include "TinySQLDb.h"
#include "storage/StoredDataManager.h"
#include "QueryProcessor.h"
#include "WebAPI.h"
#include <iostream>

int main() {
    std::cout << "Iniciando TinySQLDb...\n";

    StoredDataManager sdm;       // carga el System Catalog y reconstruye los índices
    QueryProcessor qp(sdm);
    WebAPI api(qp, 8080);

    std::cout << "Servidor escuchando en el puerto 8080...\n";
    api.run();

    return 0;
}
