import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

export default function JournalViewer({ activeSession }) {
  const [journalEntries, setJournalEntries] = useState([]);
  const [partitionId, setPartitionId] = useState(activeSession?.id || '');
  const [mountedList, setMountedList] = useState([]);
  const [loading, setLoading] = useState(false);
  const [searchFilter, setSearchFilter] = useState('');
  const [errorMsg, setErrorMsg] = useState('');

  const loadJournal = async (idToFetch) => {
    const targetId = idToFetch || partitionId || activeSession?.id;
    if (!targetId) {
      setErrorMsg('Debes especificar un ID de partición EXT3 montada.');
      return;
    }

    setLoading(true);
    setErrorMsg('');

    try {
      const res = await apiService.getJournal(targetId);
      if (res.status === 'success' && res.journaling) {
        setJournalEntries(res.journaling);
      } else {
        setJournalEntries([]);
        setErrorMsg(res.message || 'La partición especificada no es EXT3 o no contiene registros de journaling.');
      }
    } catch (err) {
      setErrorMsg(err.message || 'Error al cargar el journaling');
      setJournalEntries([]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    // Cargar la lista de particiones para selector
    apiService.getPartitions().then((res) => {
      if (res && res.partitions) {
        setMountedList(res.partitions);
        if (!partitionId && res.partitions.length > 0) {
          const firstId = res.partitions[0].id;
          setPartitionId(firstId);
          loadJournal(firstId);
        }
      }
    });

    if (activeSession?.id) {
      setPartitionId(activeSession.id);
      loadJournal(activeSession.id);
    }
  }, [activeSession]);

  const filteredEntries = journalEntries.filter((item) => {
    if (!searchFilter) return true;
    const term = searchFilter.toLowerCase();
    return (
      item.operation?.toLowerCase().includes(term) ||
      item.path?.toLowerCase().includes(term) ||
      item.content?.toLowerCase().includes(term)
    );
  });

  return (
    <div className="glass-panel" style={{ flex: 1 }}>
      <div className="panel-header">
        <div className="panel-title">
          <span>📜</span> Visualizador de Journaling (EXT3)
        </div>
        <div className="panel-actions">
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <label style={{ fontSize: '0.8rem', color: 'var(--text-secondary)' }}>ID Partición:</label>
            {mountedList.length > 0 ? (
              <select
                className="form-input"
                style={{ width: '130px', padding: '0.35rem 0.6rem', fontSize: '0.8rem' }}
                value={partitionId}
                onChange={(e) => {
                  setPartitionId(e.target.value);
                  loadJournal(e.target.value);
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
                style={{ width: '100px', padding: '0.35rem 0.6rem', fontSize: '0.8rem' }}
                placeholder="Ej: 761A"
                value={partitionId}
                onChange={(e) => setPartitionId(e.target.value)}
              />
            )}

            <button className="btn btn-secondary btn-sm" onClick={() => loadJournal(partitionId)}>
              🔄 Consultar
            </button>
          </div>
        </div>
      </div>

      <div className="panel-body">
        {/* Filtro de Búsqueda */}
        <div style={{ marginBottom: '1.2rem', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <input
            type="text"
            className="form-input"
            style={{ width: '320px' }}
            placeholder="🔍 Filtrar por operación, ruta o contenido..."
            value={searchFilter}
            onChange={(e) => setSearchFilter(e.target.value)}
          />

          <span style={{ fontSize: '0.85rem', color: 'var(--text-secondary)' }}>
            Registros encontrados: <strong>{filteredEntries.length}</strong>
          </span>
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

        {/* Tabla de Journaling */}
        <div className="table-container" style={{ flex: 1 }}>
          <table className="custom-table">
            <thead>
              <tr>
                <th>#</th>
                <th>Operación</th>
                <th>Tipo</th>
                <th>Ruta (-path)</th>
                <th>Contenido / Detalle</th>
                <th>Tamaño</th>
                <th>Fecha & Hora</th>
              </tr>
            </thead>
            <tbody>
              {loading ? (
                <tr>
                  <td colSpan="7" style={{ textAlign: 'center', padding: '2rem', color: 'var(--text-muted)' }}>
                    ⌛ Cargando registros de journaling...
                  </td>
                </tr>
              ) : filteredEntries.length === 0 ? (
                <tr>
                  <td colSpan="7" style={{ textAlign: 'center', padding: '2rem', color: 'var(--text-muted)' }}>
                    No se encontraron transacciones registradas en el journal para esta partición.
                  </td>
                </tr>
              ) : (
                filteredEntries.map((item, idx) => (
                  <tr key={idx}>
                    <td>{idx + 1}</td>
                    <td>
                      <span className={`badge ${
                        item.operation?.toLowerCase().includes('mk') ? 'badge-emerald' : 'badge-cyan'
                      }`}>
                        {item.operation}
                      </span>
                    </td>
                    <td><span className="badge badge-amber">{item.type || 'F'}</span></td>
                    <td><code>{item.path}</code></td>
                    <td style={{ color: 'var(--text-secondary)', maxWidth: '280px', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                      {item.content || '-'}
                    </td>
                    <td>{item.size} B</td>
                    <td>{item.date}</td>
                  </tr>
                ))
              )}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
