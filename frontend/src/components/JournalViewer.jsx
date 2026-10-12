import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

export default function JournalViewer({ activeSession, partitionId: propPartitionId }) {
  const [journalEntries, setJournalEntries] = useState([]);
  const [partitionId, setPartitionId] = useState(propPartitionId || activeSession?.id || '');
  const [mountedList, setMountedList] = useState([]);
  const [loading, setLoading] = useState(false);
  const [searchFilter, setSearchFilter] = useState('');
  const [errorMsg, setErrorMsg] = useState('');
  const [isExt3, setIsExt3] = useState(true);

  const loadJournal = async (idToFetch) => {
    const targetId = idToFetch || partitionId || activeSession?.id;
    if (!targetId) {
      setErrorMsg('Debes especificar o seleccionar un ID de partición EXT3 montada.');
      setJournalEntries([]);
      return;
    }

    setLoading(true);
    setErrorMsg('');

    try {
      const res = await apiService.getJournal(targetId);
      if (res.status === 'success' && res.journaling) {
        setJournalEntries(res.journaling);
        setIsExt3(true);
      } else {
        setJournalEntries([]);
        setIsExt3(false);
        setErrorMsg(res.message || 'La partición especificada no es EXT3 o no contiene registros de journaling.');
      }
    } catch (err) {
      setErrorMsg(err.message || 'Error al consultar registros de journaling');
      setJournalEntries([]);
      setIsExt3(false);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    apiService.getPartitions().then((res) => {
      if (res && res.partitions) {
        const mountedOnly = res.partitions.filter((p) => p.isMounted || p.id);
        setMountedList(mountedOnly);
        if (!partitionId && mountedOnly.length > 0) {
          const firstId = mountedOnly[0].id;
          setPartitionId(firstId);
          loadJournal(firstId);
        }
      }
    });

    if (propPartitionId) {
      setPartitionId(propPartitionId);
      loadJournal(propPartitionId);
    } else if (activeSession?.id && !partitionId) {
      setPartitionId(activeSession.id);
      loadJournal(activeSession.id);
    }
  }, [propPartitionId, activeSession]);

  const filteredEntries = journalEntries.filter((item) => {
    if (!searchFilter) return true;
    const term = searchFilter.toLowerCase();
    return (
      item.operation?.toLowerCase().includes(term) ||
      item.path?.toLowerCase().includes(term) ||
      item.content?.toLowerCase().includes(term)
    );
  });

  const getOpBadgeColor = (op) => {
    const lower = (op || '').toLowerCase();
    if (lower === 'mkdir') return 'badge-emerald';
    if (lower === 'mkfile') return 'badge-cyan';
    if (lower === 'remove') return 'badge-amber';
    if (lower === 'rename') return 'badge-cyan';
    if (lower === 'copy') return 'badge-emerald';
    if (lower === 'move') return 'badge-amber';
    if (lower === 'chown') return 'badge-cyan';
    return 'badge-cyan';
  };

  return (
    <div className="glass-panel" style={{ flex: 1, minHeight: '600px' }}>
      <div className="panel-header">
        <div className="panel-title">
          <span>📜</span> Visualizador de Journaling (EXT3 / 3fs)
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
                onKeyDown={(e) => e.key === 'Enter' && loadJournal(partitionId)}
              />
            )}

            <button className="btn btn-secondary btn-sm" onClick={() => loadJournal(partitionId)}>
              🔄 Consultar
            </button>
          </div>
        </div>
      </div>

      <div className="panel-body">
        {/* Barra de Filtro y Búsqueda */}
        <div style={{ marginBottom: '1.25rem', display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '0.75rem' }}>
          <input
            type="text"
            className="form-input"
            style={{ width: '360px', maxWidth: '100%' }}
            placeholder="🔍 Filtrar por operación (mkdir, mkfile, etc.) o ruta..."
            value={searchFilter}
            onChange={(e) => setSearchFilter(e.target.value)}
          />

          <span style={{ fontSize: '0.85rem', color: 'var(--text-secondary)' }}>
            Entradas en el Journal: <strong style={{ color: '#fff' }}>{filteredEntries.length}</strong> / 50
          </span>
        </div>

        {errorMsg && (
          <div style={{
            background: 'rgba(244, 63, 94, 0.15)',
            border: '1px solid rgba(244, 63, 94, 0.4)',
            color: 'var(--accent-rose)',
            padding: '1rem',
            borderRadius: '10px',
            fontSize: '0.875rem',
            marginBottom: '1rem'
          }}>
            <div style={{ fontWeight: 600, marginBottom: '0.2rem' }}>⚠️ Partición no compatible o sin registros</div>
            <div>{errorMsg}</div>
            {!isExt3 && (
              <div style={{ fontSize: '0.8rem', marginTop: '0.4rem', color: 'var(--text-secondary)' }}>
                Recuerda que el comando <strong>mkfs</strong> debe haberse ejecutado con el parámetro <code>-fs=3fs</code> para habilitar Journaling.
              </div>
            )}
          </div>
        )}

        {/* Tabla del Journaling */}
        <div className="table-container" style={{ flex: 1 }}>
          <table className="custom-table">
            <thead>
              <tr>
                <th style={{ width: '60px' }}>#</th>
                <th style={{ width: '130px' }}>Operación</th>
                <th>Ruta (-path)</th>
                <th>Contenido / Detalle</th>
                <th style={{ width: '180px' }}>Fecha & Hora</th>
              </tr>
            </thead>
            <tbody>
              {loading ? (
                <tr>
                  <td colSpan="5" style={{ textAlign: 'center', padding: '3rem', color: 'var(--text-muted)' }}>
                    ⌛ Leyendo el bloque de Journaling desde la partición...
                  </td>
                </tr>
              ) : filteredEntries.length === 0 ? (
                <tr>
                  <td colSpan="5" style={{ textAlign: 'center', padding: '3rem', color: 'var(--text-muted)' }}>
                    No hay entradas registradas en el journal para esta partición.
                  </td>
                </tr>
              ) : (
                filteredEntries.map((item, idx) => (
                  <tr key={idx}>
                    <td>
                      <span style={{ fontWeight: 700, color: 'var(--accent-cyan)' }}>
                        #{item.count || idx + 1}
                      </span>
                    </td>
                    <td>
                      <span className={`badge ${getOpBadgeColor(item.operation)}`}>
                        {item.operation}
                      </span>
                    </td>
                    <td>
                      <code style={{ color: '#fff', fontSize: '0.85rem' }}>{item.path}</code>
                    </td>
                    <td style={{
                      color: 'var(--text-secondary)',
                      maxWidth: '320px',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap'
                    }}>
                      {item.content || '-'}
                    </td>
                    <td style={{ fontSize: '0.8rem', color: 'var(--text-muted)' }}>
                      {item.date}
                    </td>
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
