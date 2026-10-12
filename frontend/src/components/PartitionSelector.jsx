import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

export default function PartitionSelector({ selectedDisk, onBackToDisks, onSelectPartition }) {
  const [partitions, setPartitions] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  const loadPartitions = async () => {
    setLoading(true);
    setError('');

    try {
      const diskFilter = selectedDisk ? selectedDisk.path : '';
      const res = await apiService.getPartitions(diskFilter);

      if (res.status === 'success' && res.partitions) {
        setPartitions(res.partitions);
      } else {
        setPartitions([]);
        if (res.message) setError(res.message);
      }
    } catch (err) {
      setError(err.message || 'Error al conectar con el backend');
      setPartitions([]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadPartitions();
  }, [selectedDisk]);

  const formatSize = (bytes) => {
    if (!bytes || bytes <= 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  const getTypeLabel = (type) => {
    if (type === 'P' || type === 'p') return { label: 'Primaria (P)', color: 'badge-cyan' };
    if (type === 'E' || type === 'e') return { label: 'Extendida (E)', color: 'badge-amber' };
    if (type === 'L' || type === 'l') return { label: 'Lógica (L)', color: 'badge-emerald' };
    return { label: type, color: 'badge-cyan' };
  };

  return (
    <div className="glass-panel" style={{ flex: 1 }}>
      <div className="panel-header">
        <div className="panel-title">
          <span>📦</span> Selector de Particiones
          {selectedDisk && (
            <span style={{ fontSize: '0.85rem', color: 'var(--text-muted)', fontWeight: 'normal' }}>
              &nbsp;— Disco: <code style={{ color: 'var(--accent-cyan)' }}>{selectedDisk.name || selectedDisk.path}</code>
            </span>
          )}
        </div>
        <div className="panel-actions">
          {onBackToDisks && (
            <button className="btn btn-secondary btn-sm" onClick={onBackToDisks}>
              ⬅ Volver a Discos
            </button>
          )}
          <button className="btn btn-secondary btn-sm" onClick={loadPartitions}>
            🔄 Actualizar
          </button>
        </div>
      </div>

      <div className="panel-body">
        <p style={{ color: 'var(--text-secondary)', marginBottom: '1.25rem', fontSize: '0.9rem' }}>
          Selecciona una partición montada para explorar su árbol de archivos en el navegador:
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
            ⌛ Consultando particiones...
          </div>
        ) : partitions.length === 0 ? (
          <div style={{
            padding: '2.5rem',
            textAlign: 'center',
            background: 'rgba(2, 6, 23, 0.4)',
            borderRadius: '12px',
            color: 'var(--text-muted)'
          }}>
            <div style={{ fontSize: '2.5rem', marginBottom: '0.75rem' }}>📭</div>
            <div style={{ fontWeight: 600, color: 'var(--text-primary)', marginBottom: '0.4rem' }}>
              No se encontraron particiones en este disco
            </div>
            <div style={{ fontSize: '0.85rem' }}>
              Crea particiones con <code>fdisk -type=P -size=... -name=... -path=...</code>
            </div>
          </div>
        ) : (
          <div className="grid-cards">
            {partitions.map((part, idx) => {
              const typeInfo = getTypeLabel(part.type);
              const isMounted = part.isMounted;

              return (
                <div
                  key={idx}
                  className={`disk-card ${isMounted ? 'clickable-disk' : ''}`}
                  onClick={() => {
                    if (isMounted && onSelectPartition) {
                      onSelectPartition(part);
                    }
                  }}
                  style={{
                    borderColor: isMounted ? 'rgba(56, 189, 248, 0.4)' : 'var(--border-color)',
                    background: isMounted ? 'rgba(15, 23, 42, 0.85)' : 'rgba(15, 23, 42, 0.55)'
                  }}
                >
                  <div className="disk-header">
                    <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
                      <span style={{ fontSize: '1.4rem' }}>
                        {part.type === 'E' ? '🗄️' : '📁'}
                      </span>
                      <div>
                        <div style={{ fontWeight: 700, fontSize: '1rem', color: '#fff' }}>
                          {part.name}
                        </div>
                        <div style={{ fontSize: '0.725rem', color: 'var(--text-muted)' }}>
                          Inicio: Byte #{part.start}
                        </div>
                      </div>
                    </div>
                    <span className={`badge ${typeInfo.color}`}>
                      {typeInfo.label}
                    </span>
                  </div>

                  {/* Metadatos */}
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
                      <span style={{ color: 'var(--text-muted)' }}>Tamaño:</span>{' '}
                      <strong style={{ color: '#fff' }}>{formatSize(part.size)}</strong>
                    </div>
                    <div>
                      <span style={{ color: 'var(--text-muted)' }}>Ajuste (Fit):</span>{' '}
                      <strong style={{ color: 'var(--accent-cyan)' }}>{part.fit}</strong>
                    </div>
                    <div style={{ gridColumn: 'span 2' }}>
                      <span style={{ color: 'var(--text-muted)' }}>Disco:</span>{' '}
                      <code style={{ fontSize: '0.75rem' }}>{part.diskName || part.disk}</code>
                    </div>
                  </div>

                  {/* Estado de Montaje */}
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    paddingTop: '0.6rem',
                    borderTop: '1px solid var(--border-color)',
                    fontSize: '0.825rem'
                  }}>
                    <span>Estado:</span>
                    {isMounted ? (
                      <div style={{ display: 'flex', alignItems: 'center', gap: '0.4rem' }}>
                        <span className="badge badge-emerald">● Montada</span>
                        <span className="badge badge-cyan" style={{ fontWeight: 700 }}>ID: {part.id}</span>
                      </div>
                    ) : (
                      <span className="badge" style={{ background: 'rgba(100, 116, 139, 0.2)', color: 'var(--text-muted)' }}>
                        No Montada
                      </span>
                    )}
                  </div>

                  {isMounted ? (
                    <button
                      className="btn btn-primary btn-sm"
                      style={{ width: '100%', marginTop: '0.9rem' }}
                      onClick={(e) => {
                        e.stopPropagation();
                        if (onSelectPartition) onSelectPartition(part);
                      }}
                    >
                      Explorar Archivos ({part.id}) ➜
                    </button>
                  ) : (
                    <div style={{
                      marginTop: '0.75rem',
                      textAlign: 'center',
                      fontSize: '0.75rem',
                      color: 'var(--text-muted)'
                    }}>
                      Monta esta partición con <code>mount -path=... -name={part.name}</code> para explorar.
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
}
