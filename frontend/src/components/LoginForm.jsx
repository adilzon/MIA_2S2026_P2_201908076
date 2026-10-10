import React, { useState, useEffect } from 'react';
import { apiService } from '../services/api';

export default function LoginForm({ isOpen, onClose, onLoginSuccess }) {
  const [user, setUser] = useState('root');
  const [pass, setPass] = useState('123');
  const [id, setId] = useState('');
  const [mountedPartitions, setMountedPartitions] = useState([]);
  const [loading, setLoading] = useState(false);
  const [errorMsg, setErrorMsg] = useState('');

  useEffect(() => {
    if (isOpen) {
      // Fetch mounted partitions to assist auto-fill
      apiService.getPartitions().then((res) => {
        if (res && res.partitions && res.partitions.length > 0) {
          setMountedPartitions(res.partitions);
          if (!id) setId(res.partitions[0].id);
        }
      });
    }
  }, [isOpen]);

  if (!isOpen) return null;

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!user || !pass || !id) {
      setErrorMsg('Por favor ingresa ID de Partición, Usuario y Contraseña.');
      return;
    }

    setLoading(true);
    setErrorMsg('');

    try {
      const res = await apiService.login(user, pass, id);
      setLoading(false);

      if (res.status === 'success' || res.isLoggedIn) {
        onLoginSuccess({
          isLoggedIn: true,
          user: user,
          id: id
        });
        onClose();
      } else {
        setErrorMsg(res.message || 'Error al iniciar sesión. Verifique credenciales o montaje.');
      }
    } catch (err) {
      setLoading(false);
      setErrorMsg(err.message || 'Fallo de conexión con el backend.');
    }
  };

  return (
    <div className="modal-overlay">
      <div className="modal-content">
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1.25rem' }}>
          <h3 style={{ fontSize: '1.2rem', fontWeight: 700, color: 'var(--accent-cyan)' }}>
            🔑 Iniciar Sesión (login)
          </h3>
          <button
            onClick={onClose}
            style={{ background: 'none', border: 'none', color: 'var(--text-muted)', cursor: 'pointer', fontSize: '1.2rem' }}
          >
            ✕
          </button>
        </div>

        {errorMsg && (
          <div style={{
            background: 'rgba(244, 63, 94, 0.15)',
            border: '1px solid rgba(244, 63, 94, 0.4)',
            color: 'var(--accent-rose)',
            padding: '0.75rem',
            borderRadius: '8px',
            fontSize: '0.85rem',
            marginBottom: '1rem'
          }}>
            ⚠️ {errorMsg}
          </div>
        )}

        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label className="form-label">ID de Partición Montada (-id):</label>
            {mountedPartitions.length > 0 ? (
              <select
                className="form-input"
                value={id}
                onChange={(e) => setId(e.target.value)}
              >
                {mountedPartitions.map((p) => (
                  <option key={p.id} value={p.id}>
                    {p.id} - {p.name} ({p.path})
                  </option>
                ))}
              </select>
            ) : (
              <input
                type="text"
                className="form-input"
                placeholder="Ej: 761A"
                value={id}
                onChange={(e) => setId(e.target.value)}
              />
            )}
          </div>

          <div className="form-group">
            <label className="form-label">Usuario (-usr):</label>
            <input
              type="text"
              className="form-input"
              placeholder="root"
              value={user}
              onChange={(e) => setUser(e.target.value)}
            />
          </div>

          <div className="form-group">
            <label className="form-label">Contraseña (-pwd):</label>
            <input
              type="password"
              className="form-input"
              placeholder="123"
              value={pass}
              onChange={(e) => setPass(e.target.value)}
            />
          </div>

          <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '0.75rem', marginTop: '1.5rem' }}>
            <button type="button" className="btn btn-secondary" onClick={onClose}>
              Cancelar
            </button>
            <button type="submit" className="btn btn-primary" disabled={loading}>
              {loading ? '⌛ Autenticando...' : 'Iniciar Sesión'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
