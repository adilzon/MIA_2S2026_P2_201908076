import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

export default function DiskSelector({ onSelectDisk }) {
  const [disks, setDisks] = useState([]);
  const [loading, setLoading] = useState(true);
  const [customPath, setCustomPath] = useState('');
  const [error, setError] = useState('');

  const loadDisks = async () => {
    setLoading(true);
    setError('');

    try {
      const res = await apiService.getDisks(customPath);
      if (res.status === 'success' && res.disks) {
        setDisks(res.disks);
      } else {
        setDisks([]);
        if (res.message) setError(res.message);
      }
    } catch (err) {
      setError(err.message || 'Error al conectar con el backend');
      setDisks([]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadDisks();
  }, []);

  const formatSize = (bytes) => {
    if (!bytes || bytes <= 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  return (
    <div className="glass-panel" style={{ flex: 1 }}>
      <div className="panel-header">
        <div className="panel-title">
          <span>💽</span> Selector de Discos (.mia / .dsk)
        </div>
        <div className="panel-actions">
          <input
            type="text"
            className="form-input"
            placeholder="Filtrar o ingresar ruta (ej: ./disco1.mia)"
            value={customPath}
            onChange={(e) => setCustomPath(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && loadDisks()}
            style={{ width: '280px', padding: '0.4rem 0.8rem', fontSize: '0.8rem' }}
          />
          <button className="btn btn-secondary btn-sm" onClick={loadDisks}>
            🔄 Actualizar
          </button>
        </div>
      </div>

      <div className="panel-body">
        <p style={{ color: 'var(--text-secondary)', marginBottom: '1.25rem', fontSize: '0.9rem' }}>
          Selecciona un disco haciendo clic sobre su tarjeta para explorar y seleccionar sus particiones:
        </p>

        {error && (
          <div style={{
            background: 'rgba(244, 63, 94, 0.15)',
            border: '1px solid rgba(244, 63, 94, 0.4)',
            color: 'var(--accent-rose)',
            padding: '0.8rem 1rem',
            borderRadius: '8px',
            fontSize: '0.85rem',
            marginBottom: '1rem'
          }}>
            ⚠️ {error}
          </div>
        )}

        {loading ? (
          <div style={{ padding: '3rem', textAlign: 'center', color: 'var(--text-muted)' }}>
            ⌛ Escaneando discos disponibles...
          </div>
        ) : disks.length === 0 ? (
          <div style={{
            padding: '2.5rem',
            textAlign: 'center',
            background: 'rgba(2, 6, 23, 0.4)',
            borderRadius: '12px',
            color: 'var(--text-muted)'
          }}>
            <div style={{ fontSize: '2.5rem', marginBottom: '0.75rem' }}>💿</div>
            <div style={{ fontWeight: 600, color: 'var(--text-primary)', marginBottom: '0.4rem' }}>
              No se detectaron discos creados
            </div>
            <div style={{ fontSize: '0.85rem' }}>
              Crea uno en la consola con: <code>mkdisk -size=20 -unit=M -path="disco1.mia"</code>
            </div>
          </div>
        ) : (
          <div className="grid-cards">
            {disks.map((disk, idx) => {
              const mountedCount = disk.mountedCount || (disk.mountedPartitions ? disk.mountedPartitions.length : 0);
              return (
                <div
                  key={idx}
                  className="disk-card clickable-disk"
                  onClick={() => onSelectDisk && onSelectDisk(disk)}
                >
                  <div className="disk-header">
                    <div style={{ display: 'flex', alignItems: 'center', gap: '0.6rem' }}>
                      <span className="disk-icon">💽</span>
                      <div>
                        <div style={{ fontWeight: 700, fontSize: '1rem', color: '#fff' }}>
                          {disk.name || disk.path}
                        </div>
                        <div style={{ fontSize: '0.75rem', color: 'var(--text-muted)' }}>
                          Path: <code>{disk.path}</code>
                        </div>
                      </div>
                    </div>
                    <span className="badge badge-amber">{formatSize(disk.size)}</span>
                  </div>

                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr',
                    gap: '0.6rem',
                    background: 'rgba(2, 6, 23, 0.5)',
                    padding: '0.8rem',
                    borderRadius: '8px',
                    fontSize: '0.8rem',
                    margin: '0.8rem 0'
                  }}>
                    <div>
                      <span style={{ color: 'var(--text-muted)' }}>Ajuste (Fit):</span>{' '}
                      <strong style={{ color: 'var(--accent-cyan)' }}>{disk.fit}</strong>
                    </div>
                    <div>
                      <span style={{ color: 'var(--text-muted)' }}>Signature:</span>{' '}
                      <strong style={{ color: '#fff' }}>#{disk.signature}</strong>
                    </div>
                    <div style={{ gridColumn: 'span 2' }}>
                      <span style={{ color: 'var(--text-muted)' }}>Fecha de Creación:</span>{' '}
                      <span style={{ color: 'var(--text-secondary)' }}>{disk.creationDate || 'N/A'}</span>
                    </div>
                  </div>

                  {/* Estado de Particiones Montadas */}
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    paddingTop: '0.5rem',
                    borderTop: '1px solid var(--border-color)',
                    fontSize: '0.8rem'
                  }}>
                    <span style={{ color: 'var(--text-secondary)' }}>Particiones Montadas:</span>
                    {mountedCount > 0 ? (
                      <span className="badge badge-emerald">
                        ● {mountedCount} montada{mountedCount > 1 ? 's' : ''}
                      </span>
                    ) : (
                      <span className="badge" style={{ background: 'rgba(100, 116, 139, 0.2)', color: 'var(--text-muted)' }}>
                        Ninguna
                      </span>
                    )}
                  </div>

                  {disk.mountedPartitions && disk.mountedPartitions.length > 0 && (
                    <div style={{ display: 'flex', gap: '0.4rem', flexWrap: 'wrap', marginTop: '0.5rem' }}>
                      {disk.mountedPartitions.map((mp, mIdx) => (
                        <span key={mIdx} className="badge badge-cyan" style={{ fontSize: '0.7rem' }}>
                          {mp.id} ({mp.name})
                        </span>
                      ))}
                    </div>
                  )}

                  <button
                    className="btn btn-primary btn-sm"
                    style={{ width: '100%', marginTop: '0.8rem' }}
                    onClick={(e) => {
                      e.stopPropagation();
                      if (onSelectDisk) onSelectDisk(disk);
                    }}
                  >
                    Ver Particiones ➜
                  </button>
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
}
