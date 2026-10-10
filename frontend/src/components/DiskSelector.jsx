import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

export default function DiskSelector() {
  const [disks, setDisks] = useState([]);
  const [partitions, setPartitions] = useState([]);
  const [loading, setLoading] = useState(true);
  const [customPath, setCustomPath] = useState('');
  const [error, setError] = useState('');

  const loadData = async () => {
    setLoading(true);
    setError('');

    try {
      const disksRes = await apiService.getDisks(customPath);
      const partsRes = await apiService.getPartitions();

      if (disksRes.status === 'success' && disksRes.disks) {
        setDisks(disksRes.disks);
      } else {
        setDisks([]);
      }

      if (partsRes.status === 'success' && partsRes.partitions) {
        setPartitions(partsRes.partitions);
      } else {
        setPartitions([]);
      }
    } catch (err) {
      setError(err.message || 'Error al cargar información de discos');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadData();
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
          <span>💾</span> Gestión de Discos y Particiones (MBR / EBR)
        </div>
        <div className="panel-actions">
          <input
            type="text"
            className="form-input"
            placeholder="Filtrar por path (ej: ./disco1.dsk)"
            value={customPath}
            onChange={(e) => setCustomPath(e.target.value)}
            style={{ width: '260px', padding: '0.4rem 0.8rem', fontSize: '0.8rem' }}
          />
          <button className="btn btn-secondary btn-sm" onClick={loadData}>
            🔄 Actualizar
          </button>
        </div>
      </div>

      <div className="panel-body">
        {/* Sección de Particiones Montadas */}
        <div style={{ marginBottom: '2rem' }}>
          <h4 style={{ color: 'var(--accent-cyan)', marginBottom: '0.75rem', fontSize: '1.05rem', display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <span>📌</span> Particiones Montadas Activas ({partitions.length})
          </h4>

          {partitions.length === 0 ? (
            <div style={{ padding: '1rem', background: 'rgba(2, 6, 23, 0.4)', borderRadius: '10px', color: 'var(--text-muted)', fontSize: '0.875rem' }}>
              No hay particiones montadas actualmente. Utiliza el comando <code>mount -path=... -name=...</code> en la consola.
            </div>
          ) : (
            <div className="grid-cards">
              {partitions.map((part) => (
                <div key={part.id} className="disk-card" style={{ borderColor: 'rgba(56, 189, 248, 0.3)' }}>
                  <div className="disk-header">
                    <span className="badge badge-cyan" style={{ fontSize: '0.85rem' }}>ID: {part.id}</span>
                    <span style={{ fontSize: '0.8rem', color: 'var(--accent-emerald)', fontWeight: 600 }}>● Montada</span>
                  </div>
                  <div style={{ fontSize: '0.95rem', fontWeight: 700, color: '#fff', marginBottom: '0.4rem' }}>
                    Partición: {part.name}
                  </div>
                  <div style={{ fontSize: '0.8rem', color: 'var(--text-secondary)' }}>
                    📁 Path: <code>{part.path}</code>
                  </div>
                  <div style={{ fontSize: '0.8rem', color: 'var(--text-muted)', marginTop: '0.3rem' }}>
                    Letra: <strong>{part.letter}</strong> | Num: <strong>{part.num}</strong>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Sección de Discos Creados */}
        <div>
          <h4 style={{ color: 'var(--accent-cyan)', marginBottom: '0.75rem', fontSize: '1.05rem', display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <span>💽</span> Estructura MBR de Discos ({disks.length})
          </h4>

          {loading ? (
            <div style={{ padding: '2rem', textAlign: 'center', color: 'var(--text-muted)' }}>
              ⌛ Cargando información de discos desde el backend...
            </div>
          ) : disks.length === 0 ? (
            <div style={{ padding: '1.5rem', background: 'rgba(2, 6, 23, 0.4)', borderRadius: '10px', color: 'var(--text-muted)', fontSize: '0.875rem' }}>
              No se encontraron discos montados o en la ruta indicada. Ejecuta <code>mkdisk -size=...</code> para crear un disco nuevo.
            </div>
          ) : (
            <div className="grid-cards">
              {disks.map((disk, idx) => (
                <div key={idx} className="disk-card">
                  <div className="disk-header">
                    <div style={{ display: 'flex', alignItems: 'center', gap: '0.6rem' }}>
                      <span className="disk-icon">💽</span>
                      <div>
                        <div style={{ fontWeight: 700, fontSize: '0.95rem' }}>{disk.path}</div>
                        <div style={{ fontSize: '0.75rem', color: 'var(--text-muted)' }}>Signature: #{disk.signature}</div>
                      </div>
                    </div>
                    <span className="badge badge-amber">{formatSize(disk.size)}</span>
                  </div>

                  <div style={{ fontSize: '0.8rem', color: 'var(--text-secondary)', marginBottom: '0.5rem' }}>
                    📅 Creado: {disk.creationDate} | Fit: <strong>{disk.fit}</strong>
                  </div>

                  {/* Barra Visual de Particiones */}
                  <div className="partition-bar">
                    {disk.partitions && disk.partitions.map((p, pIdx) => {
                      const percentage = Math.max(5, (p.size / disk.size) * 100);
                      const isPrimary = p.type === 'P';
                      return (
                        <div
                          key={pIdx}
                          className={`partition-segment ${isPrimary ? 'partition-primary' : 'partition-extended'}`}
                          style={{ width: `${percentage}%` }}
                          title={`${p.name} (${p.type}) - ${formatSize(p.size)}`}
                        >
                          {p.name}
                        </div>
                      );
                    })}
                  </div>

                  {/* Tabla de Particiones en MBR */}
                  <div className="table-container" style={{ marginTop: '0.8rem' }}>
                    <table className="custom-table">
                      <thead>
                        <tr>
                          <th>Nombre</th>
                          <th>Tipo</th>
                          <th>Fit</th>
                          <th>Inicio</th>
                          <th>Tamaño</th>
                        </tr>
                      </thead>
                      <tbody>
                        {disk.partitions && disk.partitions.map((p, pIdx) => (
                          <tr key={pIdx}>
                            <td><strong>{p.name}</strong></td>
                            <td><span className="badge badge-cyan">{p.type}</span></td>
                            <td>{p.fit}</td>
                            <td>{p.start}</td>
                            <td>{formatSize(p.size)}</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
