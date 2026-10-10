import React, { useRef } from 'react';

export default function CommandInput({
  commands,
  setCommands,
  onExecute,
  isLoading
}) {
  const fileInputRef = useRef(null);

  const handleFileUpload = (e) => {
    const file = e.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (event) => {
      const content = event.target.result;
      setCommands(content);
    };
    reader.readAsText(file);
  };

  const handleKeyDown = (e) => {
    // Ctrl + Enter para ejecutar
    if (e.ctrlKey && e.key === 'Enter') {
      e.preventDefault();
      onExecute();
    }
  };

  return (
    <div className="glass-panel">
      <div className="panel-header">
        <div className="panel-title">
          <span>⚡</span> Consola de Comandos (.mia / script)
        </div>
        <div className="panel-actions">
          <button
            className="btn btn-secondary btn-sm"
            onClick={() => setCommands('')}
            title="Limpiar entrada"
          >
            🗑️ Limpiar
          </button>

          <button
            className="btn btn-primary btn-sm"
            onClick={onExecute}
            disabled={isLoading || !commands.trim()}
          >
            {isLoading ? '⌛ Ejecutando...' : '▶ Exec (Ctrl+Enter)'}
          </button>
        </div>
      </div>

      <div className="panel-body">
        <div className="code-editor-container">
          <textarea
            className="command-textarea"
            placeholder="# Ingresa comandos del sistema de archivos EXT2/EXT3
# Ejemplo:
mkdisk -size=10 -u=m -path=./disco1.dsk
fdisk -type=P -u=k -name=Part1 -size=1000 -path=./disco1.dsk
mount -path=./disco1.dsk -name=Part1
mkfs -type=fast -id=761A -fs=3fs
login -usr=root -pwd=123 -id=761A
mkdir -path=/user
mkfile -path=/user/notas.txt -size=50"
            value={commands}
            onChange={(e) => setCommands(e.target.value)}
            onKeyDown={handleKeyDown}
          />

          <div className="file-upload-dropzone">
            <span style={{ fontSize: '0.85rem', color: 'var(--text-secondary)' }}>
              📄 Cargar archivo de script (<strong>.smia</strong> / <strong>.txt</strong>):
            </span>

            <input
              type="file"
              ref={fileInputRef}
              onChange={handleFileUpload}
              accept=".smia,.txt,.mia"
              style={{ display: 'none' }}
            />

            <button
              className="btn btn-secondary btn-sm"
              onClick={() => fileInputRef.current && fileInputRef.current.click()}
            >
              📁 Seleccionar Archivo
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
