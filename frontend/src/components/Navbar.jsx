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
          <span>💻</span> Consola / Terminal
        </button>

        <button
          className={`nav-tab ${activeTab === 'disks' ? 'active' : ''}`}
          onClick={() => setActiveTab('disks')}
        >
          <span>💾</span> Discos & Particiones
        </button>

        <button
          className={`nav-tab ${activeTab === 'explorer' ? 'active' : ''}`}
          onClick={() => setActiveTab('explorer')}
        >
          <span>📁</span> Explorador de Archivos
        </button>

        <button
          className={`nav-tab ${activeTab === 'journal' ? 'active' : ''}`}
          onClick={() => setActiveTab('journal')}
        >
          <span>📜</span> Journaling (EXT3)
        </button>

        <button
          className={`nav-tab ${activeTab === 'reports' ? 'active' : ''}`}
          onClick={() => setActiveTab('reports')}
        >
          <span>📊</span> Reportes Graphviz
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

        {/* Sesión de Usuario */}
        {session.isLoggedIn ? (
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.6rem' }}>
            <span style={{ fontSize: '0.85rem', color: 'var(--accent-cyan)', fontWeight: '600' }}>
              👤 {session.user} ({session.id})
            </span>
            <button className="btn btn-secondary btn-sm" onClick={onLogout}>
              Cerrar Sesión
            </button>
          </div>
        ) : (
          <button className="btn btn-primary btn-sm" onClick={onOpenLogin}>
            🔑 Iniciar Sesión
          </button>
        )}
      </div>

      {/* Modal de Configuración de URL */}
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
