import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

export default function FileVisualizer({ activeSession, partitionId: propPartitionId, onBackToPartitions }) {
  const [partitionId, setPartitionId] = useState(propPartitionId || activeSession?.id || '');
  const [mountedList, setMountedList] = useState([]);
  const [currentPath, setCurrentPath] = useState('/');
  const [items, setItems] = useState([]);
  const [loading, setLoading] = useState(false);
  const [errorMsg, setErrorMsg] = useState('');

  // Modal para ver archivo
  const [selectedFile, setSelectedFile] = useState(null);
  const [fileContentLoading, setFileContentLoading] = useState(false);

  // Cargar lista de particiones disponibles
  useEffect(() => {
    apiService.getPartitions().then((res) => {
      if (res && res.partitions) {
        // Filtrar solo las montadas
        const mountedOnly = res.partitions.filter((p) => p.isMounted || p.id);
        setMountedList(mountedOnly);

        if (!partitionId && mountedOnly.length > 0) {
          setPartitionId(mountedOnly[0].id);
        }
      }
    });
  }, []);

  useEffect(() => {
    if (propPartitionId) {
      setPartitionId(propPartitionId);
    } else if (activeSession?.id && !partitionId) {
      setPartitionId(activeSession.id);
    }
  }, [propPartitionId, activeSession]);

  // Cargar contenidos del directorio actual
  const loadDirectory = async (idToUse, pathToFetch) => {
    const targetId = idToUse || partitionId;
    if (!targetId) {
      setErrorMsg('Selecciona una partición montada para explorar sus archivos.');
      setItems([]);
      return;
    }

    setLoading(true);
    setErrorMsg('');

    try {
      const res = await apiService.getTree(targetId, pathToFetch);
      if (res.status === 'success') {
        if (res.type === 'folder') {
          setItems(res.items || []);
          setCurrentPath(res.path || pathToFetch);
        } else if (res.type === 'file') {
          // Si por alguna razón se cargó un archivo directo
          setSelectedFile(res);
        }
      } else {
        setItems([]);
        setErrorMsg(res.message || 'No se pudieron cargar los archivos de la ruta especificada.');
      }
    } catch (err) {
      setErrorMsg(err.message || 'Error de comunicación con el backend');
      setItems([]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    if (partitionId) {
      loadDirectory(partitionId, currentPath);
    }
  }, [partitionId]);

  // Navegar dentro de una carpeta
  const handleOpenFolder = (folderPath) => {
    setCurrentPath(folderPath);
    loadDirectory(partitionId, folderPath);
  };

  // Abrir y leer un archivo
  const handleOpenFile = async (item) => {
    setFileContentLoading(true);
    setSelectedFile({
      name: item.name,
      path: item.path,
      size: item.size,
      date: item.date,
      perm: item.perm,
      uid: item.uid,
      gid: item.gid,
      content: 'Cargando contenido del archivo...'
    });

    try {
      const res = await apiService.getTree(partitionId, item.path);
      if (res.status === 'success' && res.type === 'file') {
        setSelectedFile(res);
      } else {
        // Fallback a leer con cat vía execute
        const catRes = await apiService.executeCommands(`cat -file1="${item.path}"`);
        setSelectedFile((prev) => ({
          ...prev,
          content: catRes.output || '(Archivo vacío o sin contenido)'
        }));
      }
    } catch (err) {
      setSelectedFile((prev) => ({
        ...prev,
        content: `Error al leer contenido: ${err.message}`
      }));
    } finally {
      setFileContentLoading(false);
    }
  };

  // Volver a la carpeta padre
  const handleGoBack = () => {
    if (currentPath === '/' || !currentPath) return;
    const parts = currentPath.split('/').filter(Boolean);
    parts.pop();
    const newPath = parts.length === 0 ? '/' : '/' + parts.join('/');
    setCurrentPath(newPath);
    loadDirectory(partitionId, newPath);
  };

  // Volver a raíz
  const handleGoHome = () => {
    setCurrentPath('/');
    loadDirectory(partitionId, '/');
  };

  // Navegar con breadcrumbs
  const handleBreadcrumbClick = (index) => {
    if (index === -1) {
      handleGoHome();
      return;
    }
    const parts = currentPath.split('/').filter(Boolean);
    const newParts = parts.slice(0, index + 1);
    const newPath = '/' + newParts.join('/');
    setCurrentPath(newPath);
    loadDirectory(partitionId, newPath);
  };

  const breadcrumbs = currentPath.split('/').filter(Boolean);

  const getFileIcon = (filename) => {
    const ext = filename.split('.').pop().toLowerCase();
    if (ext === 'txt') return '📄';
    if (ext === 'png' || ext === 'jpg' || ext === 'jpeg') return '🖼️';
    if (ext === 'pdf') return '📕';
    if (ext === 'json' || ext === 'xml') return '⚙️';
    if (ext === 'sh' || ext === 'mia') return '📜';
    return '📄';
  };

  const formatSize = (bytes) => {
    if (!bytes || bytes <= 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  return (
    <div className="glass-panel" style={{ flex: 1, minHeight: '600px' }}>
      {/* Encabezado del Panel */}
      <div className="panel-header">
        <div className="panel-title">
          <span>📁</span> Navegador de Archivos (Modo Lectura)
        </div>
        <div className="panel-actions">
          {onBackToPartitions && (
            <button className="btn btn-secondary btn-sm" onClick={onBackToPartitions}>
              ⬅ Particiones
            </button>
          )}

          <div style={{ display: 'flex', alignItems: 'center', gap: '0.4rem' }}>
            <label style={{ fontSize: '0.8rem', color: 'var(--text-secondary)' }}>ID Partición:</label>
            {mountedList.length > 0 ? (
              <select
                className="form-input"
                style={{ width: '130px', padding: '0.35rem 0.6rem', fontSize: '0.8rem' }}
                value={partitionId}
                onChange={(e) => {
                  setPartitionId(e.target.value);
                  loadDirectory(e.target.value, '/');
                }}
              >
                {mountedList.map((p) => (
                  <option key={p.id} value={p.id}>
                    {p.id} ({p.name})
                  </option>
                ))}
              </select>
            ) : (
              <input
                type="text"
                className="form-input"
                style={{ width: '90px', padding: '0.35rem 0.6rem', fontSize: '0.8rem' }}
                placeholder="ID ej: 761A"
                value={partitionId}
                onChange={(e) => setPartitionId(e.target.value)}
                onKeyDown={(e) => e.key === 'Enter' && loadDirectory(partitionId, currentPath)}
              />
            )}
            <button className="btn btn-secondary btn-sm" onClick={() => loadDirectory(partitionId, currentPath)}>
              🔄 Refrescar
            </button>
          </div>
        </div>
      </div>

      <div className="panel-body">
        {/* Barra de Navegación: Botones de Control + Breadcrumbs */}
        <div style={{ display: 'flex', gap: '0.75rem', alignItems: 'center', marginBottom: '1.25rem' }}>
          <button
            className="btn btn-secondary btn-sm"
            onClick={handleGoHome}
            title="Ir a la raíz /"
            disabled={currentPath === '/'}
          >
            🏠 Inicio
          </button>
          <button
            className="btn btn-secondary btn-sm"
            onClick={handleGoBack}
            title="Subir de nivel"
            disabled={currentPath === '/'}
          >
            ⬅ Atrás
          </button>

          {/* Breadcrumb Path Bar */}
          <div className="breadcrumb-bar" style={{ flex: 1 }}>
            <span
              className="breadcrumb-crumb"
              onClick={() => handleBreadcrumbClick(-1)}
              style={{ fontWeight: currentPath === '/' ? 700 : 500 }}
            >
              root (/)
            </span>
            {breadcrumbs.map((segment, idx) => (
              <React.Fragment key={idx}>
                <span className="breadcrumb-separator">/</span>
                <span
                  className="breadcrumb-crumb"
                  onClick={() => handleBreadcrumbClick(idx)}
                  style={{
                    fontWeight: idx === breadcrumbs.length - 1 ? 700 : 500,
                    color: idx === breadcrumbs.length - 1 ? '#fff' : 'var(--accent-cyan)'
                  }}
                >
                  {segment}
                </span>
              </React.Fragment>
            ))}
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

        {/* Contenedor Principal de Archivos y Carpetas */}
        {loading ? (
          <div style={{ padding: '4rem', textAlign: 'center', color: 'var(--text-muted)' }}>
            ⌛ Leyendo inodos y bloques del sistema de archivos...
          </div>
        ) : !partitionId ? (
          <div style={{ padding: '3rem', textAlign: 'center', color: 'var(--text-muted)' }}>
            Selecciona una partición montada en la parte superior para comenzar a navegar.
          </div>
        ) : items.length === 0 ? (
          <div style={{
            padding: '3rem',
            textAlign: 'center',
            background: 'rgba(2, 6, 23, 0.4)',
            borderRadius: '12px',
            color: 'var(--text-muted)'
          }}>
            <div style={{ fontSize: '3rem', marginBottom: '0.5rem' }}>📂</div>
            <div style={{ fontWeight: 600, color: 'var(--text-primary)', marginBottom: '0.3rem' }}>
              Carpeta vacía
            </div>
            <div style={{ fontSize: '0.85rem' }}>
              No hay archivos ni subcarpetas en <code>{currentPath}</code>
            </div>
          </div>
        ) : (
          <div className="file-grid">
            {items.map((item, idx) => {
              const isFolder = item.type === 'folder';

              return (
                <div
                  key={idx}
                  className="file-item"
                  onClick={() => {
                    if (isFolder) handleOpenFolder(item.path);
                    else handleOpenFile(item);
                  }}
                  title={`${item.name} (${isFolder ? 'Carpeta' : formatSize(item.size)})`}
                >
                  <div className="file-item-icon">
                    {isFolder ? '📁' : getFileIcon(item.name)}
                  </div>
                  <div className="file-item-name">
                    {item.name}
                  </div>
                  <div className="file-item-meta">
                    {isFolder ? 'Carpeta' : formatSize(item.size)}
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* Modal para Visualizar Contenido de Archivo */}
      {selectedFile && (
        <div className="modal-overlay" onClick={() => setSelectedFile(null)}>
          <div
            className="modal-content"
            style={{ maxWidth: '750px', width: '92%', maxHeight: '85vh', display: 'flex', flexDirection: 'column' }}
            onClick={(e) => e.stopPropagation()}
          >
            {/* Header del Modal */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '1rem' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '0.6rem' }}>
                <span style={{ fontSize: '1.8rem' }}>{getFileIcon(selectedFile.name || '')}</span>
                <div>
                  <h3 style={{ color: '#fff', fontSize: '1.15rem', wordBreak: 'break-all' }}>
                    {selectedFile.name}
                  </h3>
                  <div style={{ fontSize: '0.75rem', color: 'var(--text-muted)' }}>
                    Ruta: <code>{selectedFile.path}</code>
                  </div>
                </div>
              </div>
              <button
                className="btn btn-secondary btn-sm"
                onClick={() => setSelectedFile(null)}
                style={{ borderRadius: '50%', width: '32px', height: '32px', padding: 0, textAlign: 'center' }}
              >
                ✕
              </button>
            </div>

            {/* Metadatos del archivo */}
            <div style={{
              display: 'flex',
              gap: '1rem',
              background: 'rgba(2, 6, 23, 0.6)',
              padding: '0.6rem 0.9rem',
              borderRadius: '8px',
              fontSize: '0.75rem',
              color: 'var(--text-secondary)',
              marginBottom: '1rem',
              flexWrap: 'wrap'
            }}>
              <span>Tamaño: <strong style={{ color: '#fff' }}>{formatSize(selectedFile.size)}</strong></span>
              <span>Permisos: <strong style={{ color: 'var(--accent-cyan)' }}>{selectedFile.perm || '664'}</strong></span>
              <span>UID: <strong style={{ color: '#fff' }}>{selectedFile.uid ?? '-'}</strong></span>
              <span>GID: <strong style={{ color: '#fff' }}>{selectedFile.gid ?? '-'}</strong></span>
              {selectedFile.date && <span>Modificado: <strong>{selectedFile.date}</strong></span>}
            </div>

            {/* Contenido del archivo */}
            <div style={{
              flex: 1,
              background: '#030712',
              borderRadius: '10px',
              border: '1px solid var(--border-color)',
              padding: '1.2rem',
              fontFamily: 'var(--font-mono)',
              fontSize: '0.875rem',
              color: '#e2e8f0',
              whiteSpace: 'pre-wrap',
              overflowY: 'auto',
              maxHeight: '380px',
              lineHeight: 1.6
            }}>
              {fileContentLoading ? (
                <span style={{ color: 'var(--accent-cyan)' }}>⌛ Leyendo bloques del archivo...</span>
              ) : (
                selectedFile.content || <em style={{ color: 'var(--text-muted)' }}>(Archivo vacío)</em>
              )}
            </div>

            {/* Acciones del Modal */}
            <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '0.6rem', marginTop: '1.2rem' }}>
              <button
                className="btn btn-secondary btn-sm"
                onClick={() => {
                  navigator.clipboard.writeText(selectedFile.content || '');
                }}
              >
                📋 Copiar Texto
              </button>
              <button className="btn btn-primary btn-sm" onClick={() => setSelectedFile(null)}>
                Cerrar
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
