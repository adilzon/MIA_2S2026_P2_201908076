import React, { useRef, useEffect } from 'react';

// Formateador simple de secuencias ANSI escape (\x1B[1;32m, \x1B[0m, etc.) a elementos React
function renderFormattedAnsi(text) {
  if (!text) return <span style={{ color: 'var(--text-muted)' }}>Esperando ejecución de comandos...</span>;

  // Regex para capturar secuencias de color ANSI
  const parts = text.split(/(\x1B\[[0-9;]*m)/g);
  let currentColorClass = 'ansi-green';

  return parts.map((part, index) => {
    if (part.startsWith('\x1B[')) {
      if (part.includes('0;31m') || part.includes('1;31m') || part.includes('41m')) {
        currentColorClass = 'ansi-red';
      } else if (part.includes('0;32m') || part.includes('1;92m')) {
        currentColorClass = 'ansi-green';
      } else if (part.includes('1;34m') || part.includes('D4E6F1')) {
        currentColorClass = 'ansi-cyan';
      } else if (part.includes('33m')) {
        currentColorClass = 'ansi-yellow';
      } else if (part.includes('0m')) {
        currentColorClass = 'ansi-green';
      }
      return null;
    }
    return (
      <span key={index} className={currentColorClass}>
        {part}
      </span>
    );
  });
}

export default function CommandOutput({ output, onClear, executionTime }) {
  const terminalRef = useRef(null);

  useEffect(() => {
    if (terminalRef.current) {
      terminalRef.current.scrollTop = terminalRef.current.scrollHeight;
    }
  }, [output]);

  const handleCopy = () => {
    if (output) {
      navigator.clipboard.writeText(output);
      alert('¡Salida copiada al portapapeles!');
    }
  };

  return (
    <div className="glass-panel">
      <div className="panel-header">
        <div className="panel-title">
          <span>📟</span> Salida de Consola (Backend Log)
        </div>
        <div className="panel-actions">
          {executionTime && (
            <span style={{ fontSize: '0.75rem', color: 'var(--text-muted)' }}>
              ⏱️ {executionTime} ms
            </span>
          )}
          <button className="btn btn-secondary btn-sm" onClick={handleCopy} disabled={!output}>
            📋 Copiar
          </button>
          <button className="btn btn-secondary btn-sm" onClick={onClear}>
            🧹 Limpiar
          </button>
        </div>
      </div>

      <div className="panel-body" style={{ padding: 0 }}>
        <div className="terminal-window">
          <div className="terminal-header-bar">
            <div className="terminal-dots">
              <span className="dot dot-red"></span>
              <span className="dot dot-yellow"></span>
              <span className="dot dot-green"></span>
            </div>
            <span style={{ fontSize: '0.75rem', color: 'var(--text-muted)', fontFamily: 'var(--font-mono)' }}>
              bash - proyecto2_server@localhost:8080
            </span>
          </div>

          <div className="terminal-output-body" ref={terminalRef}>
            {renderFormattedAnsi(output)}
          </div>
        </div>
      </div>
    </div>
  );
}