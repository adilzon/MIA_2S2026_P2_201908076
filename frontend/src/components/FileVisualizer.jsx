import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

export default function FileVisualizer({ activeSession }) {
  const [filePath, setFilePath] = useState('/users.txt');
  const [fileContent, setFileContent] = useState('');
  const [loading, setLoading] = useState(false);
  const [errorMsg, setErrorMsg] = useState('');
  const [partitionId, setPartitionId] = useState(activeSession?.id || '');
  const [mountedList, setMountedList] = useState([]);

  useEffect(() => {
    apiService.getPartitions().then((res) => {
      if (res && res.partitions) {
        setMountedList(res.partitions);
        if (!partitionId && res.partitions.length > 0) {
          setPartitionId(res.partitions[0].id);
        }
      }
    });

    if (activeSession?.id) {
      setPartitionId(activeSession.id);
    }
  }, [activeSession]);

  const handleReadFile = async () => {
    if (!filePath.trim()) return;
    if (!partitionId) {
      setErrorMsg('Debes seleccionar un ID de partición montada e iniciar sesión.');
      return;
    }

    setLoading(true);
    setErrorMsg('');
    setFileContent('');

    try {
      // Usar el comando cat vía /api/execute para leer el archivo del inodo
      const cmd = `cat -file1="${filePath}"`;
      const res = await apiService.executeCommands(cmd);

      if (res.output) {
        setFileContent(res.output);
      } else {
        setErrorMsg('El archivo no pudo ser leído o se encuentra vacío.');
      }
    } catch (err) {
      setErrorMsg(err.message || 'Error al comunicarse con el backend');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="glass-panel" style={{ flex: 1 }}>
      <div className="panel-header">
        <div className="panel-title">
          <span>📁</span> Explorador y Lector de Archivos (cat)
        </div>
        <div className="panel-actions">
          <button className="btn btn-primary btn-sm" onClick={handleReadFile} disabled={loading}>
            {loading ? '⌛ Leendo...' : '📖 Leer Archivo (cat)'}
          </button>
        </div>
      </div>

      <div className="panel-body">
        {/* Controles de Entrada de Ruta */}
        <div style={{ display: 'flex', gap: '1rem', alignItems: 'center', marginBottom: '1.25rem' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <label className="form-label" style={{ margin: 0 }}>ID Partición:</label>
            {mountedList.length > 0 ? (
              <select
                className="form-input"
                style={{ width: '120px', padding: '0.4rem' }}
                value={partitionId}
                onChange={(e) => setPartitionId(e.target.value)}
              >
                {mountedList.map((p) => (
                  <option key={p.id} value={p.id}>{p.id}</option>
                ))}
              </select>
            ) : (
              <input
                type="text"
                className="form-input"
                style={{ width: '90px', padding: '0.4rem' }}
                value={partitionId}
                onChange={(e) => setPartitionId(e.target.value)}
              />
            )}
          </div>

          <div style={{ flex: 1, display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <label className="form-label" style={{ margin: 0 }}>Ruta del Archivo (-file):</label>
            <input
              type="text"
              className="form-input"
              value={filePath}
              onChange={(e) => setFilePath(e.target.value)}
              placeholder="/users.txt"
              onKeyDown={(e) => e.key === 'Enter' && handleReadFile()}
            />
          </div>
        </div>

        {errorMsg && (
          <div style={{
            background: 'rgba(244, 63, 94, 0.15)',
            border: '1px solid rgba(244, 63, 94, 0.4)',
            color: 'var(--accent-rose)',
            padding: '0.8rem 1rem',
            borderRadius: '8px',
            fontSize: '0.85rem',
            marginBottom: '1rem'
          }}>
            ⚠️ {errorMsg}
          </div>
        )}

        {/* Visor de Contenido del Archivo */}
        <div style={{
          flex: 1,
          background: '#030712',
          borderRadius: '10px',
          border: '1px solid var(--border-color)',
          padding: '1.2rem',
          fontFamily: 'var(--font-mono)',
          fontSize: '0.9rem',
          color: '#e2e8f0',
          whiteSpace: 'pre-wrap',
          overflowY: 'auto',
          minHeight: '300px'
        }}>
          {loading ? (
            <span style={{ color: 'var(--accent-cyan)' }}>⌛ Leyendo contenido del inodo...</span>
          ) : fileContent ? (
            fileContent
          ) : (
            <span style={{ color: 'var(--text-muted)' }}>
              Ingresa la ruta de un archivo (ej: <code>/users.txt</code>) y presiona <strong>Leer Archivo</strong> para mostrar su contenido.
            </span>
          )}
        </div>
      </div>
    </div>
  );
}
