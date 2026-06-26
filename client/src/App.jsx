import { useState } from "react";
import SqlEditor from "./SqlEditor.jsx";
import ResultTable from "./ResultTable";
import StatusBar from "./StatusBar";
import { executeStatement } from "./api";
import "./App.css";

export default function App() {
  const [currentDb, setCurrentDb] = useState("");
  const [results, setResults] = useState([]); // un resultado por cada sentencia ejecutada
  const [isRunning, setIsRunning] = useState(false);

  // Separa el script en sentencias individuales, usando ";" como delimitador.
  // Ignora sentencias vacías (por ejemplo, si el script termina con ";" suelto).
  function splitIntoStatements(script) {
    return script
        .split(";")
        .map((s) => s.trim())
        .filter((s) => s.length > 0);
  }

  async function handleRunScript(script) {
    const statements = splitIntoStatements(script);
    if (statements.length === 0) return;

    setIsRunning(true);
    setResults([]);

    let activeDb = currentDb; // contexto de DB que se va actualizando sentencia por sentencia
    const newResults = [];

    for (const sql of statements) {
      const result = await executeStatement(sql, activeDb);

      // Si la sentencia fue un SET DATABASE exitoso, actualizamos el contexto
      // para que las sentencias SIGUIENTES de este mismo script ya lo usen.
      if (result.success && result.message?.startsWith("DATABASE:")) {
        activeDb = result.message.substring("DATABASE:".length);
      }

      newResults.push({ sql, ...result });
    }

    setResults(newResults);
    setCurrentDb(activeDb); // persistimos el contexto final para el próximo script que ejecute el usuario
    setIsRunning(false);
  }

  return (
      <div className="app">
        <header className="app-header">
          <h1>TinySQLDb</h1>
          <span className="current-db">
          {currentDb ? `Base de datos: ${currentDb}` : "Sin base de datos activa"}
        </span>
        </header>

        <SqlEditor onRun={handleRunScript} disabled={isRunning} />

        <div className="results">
          {results.map((result, idx) => (
              <div key={idx} className="result-block">
                <div className="result-sql">{result.sql}</div>
                <StatusBar
                    success={result.success}
                    message={result.message}
                    elapsedMs={result.elapsedMs}
                />
                {result.success && result.columns?.length > 0 && (
                    <ResultTable columns={result.columns} rows={result.rows} />
                )}
              </div>
          ))}
        </div>
      </div>
  );
}