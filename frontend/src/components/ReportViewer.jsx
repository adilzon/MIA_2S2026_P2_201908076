import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

const REPORT_TYPES = [
  { id: 'mbr', label: 'Reporte MBR (Tabla de Particiones)' },
  { id: 'tree', label: 'Reporte Árbol de Inodos y Bloques (TREE)' },
  { id: 'inode', label: 'Reporte de Inodos (INODE)' },
  { id: 'block', label: 'Reporte de Bloques (BLOCK)' },
  { id: 'bm_inode', label: 'Bitmap de Inodos (BM_INODE)' },
  { id: 'bm_block', label: 'Bitmap de Bloques (BM_BLOCK)' },
  { id: 'sb', label: 'Superbloque (SB)' },
  { id: 'journaling', label: 'Journaling (EXT3)' },
  { id: 'ebr', label: 'Reporte EBR (Particiones Lógicas)' }
];

export default function ReportViewer({ activeSession }) {
  const [reportType, setReportType] = useState('mbr');
  const [partitionId, setPartitionId] = useState(activeSession?.id || '');
  const [outputPath, setOutputPath] = useState('./reporte_visual.jpg');
  const [pathFileLs, setPathFileLs] = useState('');
  const [mountedList, setMountedList] = useState([]);
  const [imageUrl, setImageUrl] = useState('');
  const [loading, setLoading] = useState(false);
  const [logOutput, setLogOutput] = useState('');
  const [errorMsg, setErrorMsg] = useState('');
  const [isModalOpen, setIsModalOpen] = useState(false);

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

  const handleGenerateReport = async () => {
    if (!partitionId) {
      setErrorMsg('Debes seleccionar o ingresar un ID de partición montada (-id).');
      return;
    }

    setLoading(true);
    setErrorMsg('');
    setImageUrl('');
    setLogOutput('');

    try {
      let cmd = `rep -id=${partitionId} -path=${outputPath} -name=${reportType}`;
      if (pathFileLs && (reportType === 'file' || reportType === 'ls')) {
        cmd += ` -path_file_ls=${pathFileLs}`;
      }

      const res = await apiService.executeCommands(cmd);
      setLogOutput(res.output || '');

      if (res.status === 'success') {
        // Cargar imagen generada
        const fullUrl = apiService.getFileUrl(outputPath) + `&t=${new Date().getTime()}`;
        setImageUrl(fullUrl);
      } else {
        setErrorMsg('El servidor no pudo generar el reporte. Verifica Graphviz instalado en el sistema.');
      }
    } catch (err) {
      setErrorMsg(err.message || 'Error al conectar con el servidor backend');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="glass-panel" style={{ flex: 1 }}>
      <div className="panel-header">
        <div className="panel-title">
          <span>📊</span> Generador & Visualizador de Reportes Graphviz (rep)
        </div>
        <div className="panel-actions">
          <button
            className="btn btn-primary btn-sm"
            onClick={handleGenerateReport}
            disabled={loading}
          >
            {loading ? '⌛ Generando...' : '⚡ Generar Reporte'}
          </button>
        </div>
      </div>

      <div className="panel-body">
        {/* Controles del Formulario de Reportes */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(220px, 1fr))',
          gap: '1rem',
          padding: '1.25rem',
          background: 'rgba(2, 6, 23, 0.5)',
          borderRadius: '12px',
          border: '1px solid var(--border-color)',
          marginBottom: '1.5rem'
        }}>
          <div>
            <label className="form-label">Tipo de Reporte (-name):</label>
            <select
              className="form-input"
              value={reportType}
              onChange={(e) => setReportType(e.target.value)}
            >
              {REPORT_TYPES.map((t) => (
                <option key={t.id} value={t.id}>
                  {t.label}
                </option>
              ))}
            </select>
          </div>

          <div>
            <label className="form-label">ID Partición (-id):</label>
            {mountedList.length > 0 ? (
              <select
                className="form-input"
                value={partitionId}
                onChange={(e) => setPartitionId(e.target.value)}
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
                placeholder="Ej: 761A"
                value={partitionId}
                onChange={(e) => setPartitionId(e.target.value)}
              />
            )}
          </div>

          <div>
            <label className="form-label">Ruta Imagen de Salida (-path):</label>
            <input
              type="text"
              className="form-input"
              value={outputPath}
              onChange={(e) => setOutputPath(e.target.value)}
              placeholder="./reporte.jpg"
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

        {logOutput && (
          <div style={{
            padding: '0.8rem',
            background: '#030712',
            borderRadius: '8px',
            fontFamily: 'var(--font-mono)',
            fontSize: '0.8rem',
            color: '#4ade80',
            marginBottom: '1.2rem',
            whiteSpace: 'pre-wrap'
          }}>
            {logOutput}
          </div>
        )}

        {/* Visualizador de la Imagen Generada */}
        <div style={{
          flex: 1,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          background: 'rgba(2, 6, 23, 0.7)',
          borderRadius: '12px',
          border: '1px dashed var(--border-color)',
          minHeight: '350px',
          padding: '1.5rem',
          position: 'relative'
        }}>
          {loading ? (
            <div style={{ textAlign: 'center', color: 'var(--accent-cyan)' }}>
              <div style={{ fontSize: '2rem', marginBottom: '0.5rem' }}>⌛</div>
              <div>Ejecutando Graphviz dot para renderizar el diagrama...</div>
            </div>
          ) : imageUrl ? (
            <div style={{ width: '100%', display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
              <img
                src={imageUrl}
                alt="Reporte Graphviz"
                style={{
                  maxWidth: '100%',
                  maxHeight: '520px',
                  objectFit: 'contain',
                  borderRadius: '8px',
                  boxShadow: '0 8px 30px rgba(0,0,0,0.5)',
                  cursor: 'pointer'
                }}
                onClick={() => setIsModalOpen(true)}
              />
              <div style={{ marginTop: '1rem', display: 'flex', gap: '0.5rem' }}>
                <button className="btn btn-secondary btn-sm" onClick={() => setIsModalOpen(true)}>
                  🔍 Ver en Pantalla Completa
                </button>
                <a className="btn btn-primary btn-sm" href={imageUrl} download="reporte.jpg" target="_blank" rel="noreferrer">
                  ⬇️ Descargar Imagen
                </a>
              </div>
            </div>
          ) : (
            <div style={{ textAlign: 'center', color: 'var(--text-muted)' }}>
              <div style={{ fontSize: '2.5rem', marginBottom: '0.5rem' }}>📉</div>
              <div>Selecciona el tipo de reporte y presiona <strong>Generar Reporte</strong> para visualizarlo aquí.</div>
            </div>
          )}
        </div>
      </div>

      {/* Modal Zoom Fullscreen */}
      {isModalOpen && imageUrl && (
        <div className="modal-overlay" onClick={() => setIsModalOpen(false)}>
          <div
            style={{
              position: 'relative',
              maxWidth: '95vw',
              maxHeight: '95vh',
              overflow: 'auto',
              background: '#090d16',
              padding: '1rem',
              borderRadius: '12px'
            }}
            onClick={(e) => e.stopPropagation()}
          >
            <button
              onClick={() => setIsModalOpen(false)}
              style={{
                position: 'absolute',
                top: '15px',
                right: '15px',
                background: 'rgba(0,0,0,0.7)',
                color: '#fff',
                border: 'none',
                borderRadius: '50%',
                width: '36px',
                height: '36px',
                cursor: 'pointer',
                fontSize: '1.2rem',
                zIndex: 10
              }}
            >
              ✕
            </button>
            <img src={imageUrl} alt="Reporte ampliado" style={{ maxWidth: '100%', height: 'auto' }} />
          </div>
        </div>
      )}
    </div>
  );
}
