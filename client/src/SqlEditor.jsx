import { useState } from "react";

export default function SqlEditor({ onRun, disabled }) {
    const [script, setScript] = useState("");

    function handleRunClick() {
        onRun(script);
    }

    return (
        <div className="sql-editor">
      <textarea
          className="sql-textarea"
          placeholder="Escribe tu script SQL aquí. Separa varias sentencias con ';'"
          value={script}
          onChange={(e) => setScript(e.target.value)}
          rows={10}
          disabled={disabled}
      />
            <button
                className="run-button"
                onClick={handleRunClick}
                disabled={disabled || script.trim().length === 0}
            >
                {disabled ? "Ejecutando..." : "Ejecutar"}
            </button>
        </div>
    );
}