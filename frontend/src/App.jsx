import React, { useState, useEffect } from 'react';
import Navbar from './components/Navbar';
import CommandInput from './components/CommandInput';
import CommandOutput from './components/CommandOutput';
import LoginForm from './components/LoginForm';
import DiskSelector from './components/DiskSelector';
import FileVisualizer from './components/FileVisualizer';
import JournalViewer from './components/JournalViewer';
import ReportViewer from './components/ReportViewer';
import { apiService } from './services/api';

export default function App() {
  const [activeTab, setActiveTab] = useState('terminal');
  const [commands, setCommands] = useState('');
  const [output, setOutput] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [executionTime, setExecutionTime] = useState(null);

  const [session, setSession] = useState({
    isLoggedIn: false,
    user: '',
    id: ''
  });

  const [isBackendOnline, setIsBackendOnline] = useState(false);
  const [isLoginOpen, setIsLoginOpen] = useState(false);

  // Verificar estado del servidor y sesión al cargar la página
  const checkBackendAndSession = async () => {
    try {
      const sess = await apiService.getSession();
      setIsBackendOnline(true);
      if (sess && sess.isLoggedIn) {
        setSession({
          isLoggedIn: true,
          user: sess.user,
          id: sess.id
        });
      } else {
        setSession({ isLoggedIn: false, user: '', id: '' });
      }
    } catch (err) {
      setIsBackendOnline(false);
    }
  };

  useEffect(() => {
    checkBackendAndSession();
    const interval = setInterval(checkBackendAndSession, 10000);
    return () => clearInterval(interval);
  }, []);

  // Ejecutar comandos en la consola
  const handleExecute = async () => {
    if (!commands.trim()) return;

    setIsLoading(true);
    const startTime = performance.now();

    try {
      const res = await apiService.executeCommands(commands);
      const endTime = performance.now();
      setExecutionTime((endTime - startTime).toFixed(0));

      if (res.output) {
        setOutput((prev) => (prev ? prev + '\n\n' + res.output : res.output));
      } else if (res.message) {
        setOutput((prev) => (prev ? prev + '\n\n' + res.message : res.message));
      } else {
        setOutput((prev) => prev + '\n\n[Servidor completó la ejecución]');
      }

      // Actualizar sesión tras ejecutar comandos (ej: si se ejecutó login o logout)
      checkBackendAndSession();
    } catch (err) {
      setOutput((prev) => (prev ? prev + '\n\n[ERROR]: ' + err.message : '[ERROR]: ' + err.message));
    } finally {
      setIsLoading(false);
    }
  };

  // Cerrar sesión
  const handleLogout = async () => {
    await apiService.logout();
    setSession({ isLoggedIn: false, user: '', id: '' });
    setOutput((prev) => prev + '\n\n[LOGOUT]: Sesión cerrada exitosamente.');
  };

  return (
    <div className="app-container">
      {/* Navbar Superior */}
      <Navbar
        activeTab={activeTab}
        setActiveTab={setActiveTab}
        session={session}
        isBackendOnline={isBackendOnline}
        onOpenLogin={() => setIsLoginOpen(true)}
        onLogout={handleLogout}
      />

      {/* Contenido Principal según la pestaña activa */}
      <main className="main-content">
        {activeTab === 'terminal' && (
          <div className="terminal-grid">
            <CommandInput
              commands={commands}
              setCommands={setCommands}
              onExecute={handleExecute}
              isLoading={isLoading}
            />
            <CommandOutput
              output={output}
              onClear={() => setOutput('')}
              executionTime={executionTime}
            />
          </div>
        )}

        {activeTab === 'disks' && (
          <DiskSelector />
        )}

        {activeTab === 'explorer' && (
          <FileVisualizer activeSession={session} />
        )}

        {activeTab === 'journal' && (
          <JournalViewer activeSession={session} />
        )}

        {activeTab === 'reports' && (
          <ReportViewer activeSession={session} />
        )}
      </main>

      {/* Modal de Iniciar Sesión */}
      <LoginForm
        isOpen={isLoginOpen}
        onClose={() => setIsLoginOpen(false)}
        onLoginSuccess={(newSession) => {
          setSession(newSession);
          setOutput((prev) => prev + `\n\n[LOGIN]: Sesión iniciada como "${newSession.user}" (-id=${newSession.id})`);
        }}
      />
    </div>
  );
}
