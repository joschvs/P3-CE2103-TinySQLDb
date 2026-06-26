
// Capa de comunicación con el servidor TinySQLDb (C++).
// Solo sabe hablar HTTP: envía UNA sentencia SQL y retorna la respuesta parseada.
// No sabe nada sobre cómo se separan los scripts en sentencias (eso es responsabilidad de App.jsx).

const API_URL = "http://localhost:8080/query";

/**
 * Envía una sentencia SQL individual al servidor.
 * @param {string} sql - una sola sentencia SQL (sin ";" al final)
 * @param {string} database - la base de datos actualmente activa (puede ser "")
 * @returns {Promise<{success: boolean, message: string, elapsedMs: number, columns: string[], rows: string[][]}>}
 */
export async function executeStatement(sql, database) {
    try {
        const response = await fetch(API_URL, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ sql, database }),
        });

        const data = await response.json();
        return data;

    } catch (error) {
        // Error de red (servidor no disponible, etc.) - no es un error del SQL en sí
        return {
            success: false,
            message: `Error de conexión: ${error.message}`,
            elapsedMs: 0,
            columns: [],
            rows: [],
        };
    }
}