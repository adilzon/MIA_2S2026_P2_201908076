import React, { useState } from 'react';
import { setApiBaseUrl, getApiBaseUrl } from '../services/api';

export default function Navbar({
  activeTab,
  setActiveTab,
  session,
  isBackendOnline,
  onOpenLogin,
  onLogout
}) {
  const [showConfig, setShowConfig] = useState(false);
  const [tempUrl, setTempUrl] = useState(getApiBaseUrl());

  const handleSaveConfig = () => {
    if (tempUrl.trim()) {
      setApiBaseUrl(tempUrl.trim());
      setShowConfig(false);
      window.location.reload();
    }
  };

  return (
    <nav className="navbar">
      <div className="nav-brand">
        <span className="brand-icon">🐧</span>
        <div>
          <div className="brand-title">EXT2 / EXT3 Studio</div>
          <div className="brand-subtitle">MIA 2S2026 - Proyecto 2</div>
        </div>
      </div>

      <div className="nav-tabs">
        <button
          className={`nav-tab ${activeTab === 'terminal' ? 'active' : ''}`}
          onClick={() => setActiveTab('terminal')}
        >
          <span>💻</span> Consola
        </button>

        <button
          className={`nav-tab ${activeTab === 'disks' ? 'active' : ''}`}
          onClick={() => setActiveTab('disks')}
        >
          <span>💽</span> Discos
        </button>

        <button
          className={`nav-tab ${activeTab === 'partitions' ? 'active' : ''}`}
          onClick={() => setActiveTab('partitions')}
        >
          <span>📦</span> Particiones
        </button>

        <button
          className={`nav-tab ${activeTab === 'explorer' ? 'active' : ''}`}
          onClick={() => setActiveTab('explorer')}
        >
          <span>📁</span> Explorador
        </button>

        <button
          className={`nav-tab ${activeTab === 'journal' ? 'active' : ''}`}
          onClick={() => setActiveTab('journal')}
        >
          <span>📜</span> Journaling
        </button>

        <button
          className={`nav-tab ${activeTab === 'reports' ? 'active' : ''}`}
          onClick={() => setActiveTab('reports')}
        >
          <span>📊</span> Reportes
        </button>
      </div>

      <div className="nav-actions">
        {/* Indicador de Estado Backend */}
        <div
          className={`status-pill ${isBackendOnline ? '' : 'offline'}`}
          title="Haz clic para configurar URL Backend"
          onClick={() => setShowConfig(true)}
          style={{ cursor: 'pointer' }}
        >
          <span className="status-dot"></span>
          <span>{isBackendOnline ? 'Backend 8080' : 'Desconectado'}</span>
        </div>

        {/* Sesión de Usuario & Botón Cerrar Sesión */}
        {session.isLoggedIn ? (
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.6rem' }}>
            <span style={{
              fontSize: '0.85rem',
              color: 'var(--accent-cyan)',
              fontWeight: '600',
              background: 'rgba(56, 189, 248, 0.1)',
              padding: '0.3rem 0.6rem',
              borderRadius: '6px',
              border: '1px solid rgba(56, 189, 248, 0.25)'
            }}>
              👤 {session.user} <span style={{ color: 'var(--text-muted)' }}>({session.id})</span>
            </span>
            <button
              className="btn btn-secondary btn-sm"
              onClick={onLogout}
              style={{
                borderColor: 'rgba(244, 63, 94, 0.3)',
                color: 'var(--accent-rose)'
              }}
              title="Cerrar la sesión actual"
            >
              🚪 Cerrar Sesión
            </button>
          </div>
        ) : (
          <button className="btn btn-primary btn-sm" onClick={onOpenLogin}>
            🔑 Iniciar Sesión
          </button>
        )}
      </div>

      {/* Modal de Configuración de URL Backend */}
      {showConfig && (
        <div className="modal-overlay">
          <div className="modal-content">
            <h3 style={{ marginBottom: '1rem', color: 'var(--accent-cyan)' }}>⚙️ Configuración del Backend</h3>
            <div className="form-group">
              <label className="form-label">URL Servidor C++ HTTP:</label>
              <input
                type="text"
                className="form-input"
                value={tempUrl}
                onChange={(e) => setTempUrl(e.target.value)}
                placeholder="http://localhost:8080"
              />
            </div>
            <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '0.5rem', marginTop: '1.5rem' }}>
              <button className="btn btn-secondary" onClick={() => setShowConfig(false)}>
                Cancelar
              </button>
              <button className="btn btn-primary" onClick={handleSaveConfig}>
                Guardar
              </button>
            </div>
          </div>
        </div>
      )}
    </nav>
  );
}
