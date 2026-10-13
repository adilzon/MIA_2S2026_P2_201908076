import axios from 'axios';

const CLOUD_API_BASE_URL = import.meta.env.VITE_API_URL || 'http://52.14.118.143:8080';
const isLocalFrontend = ['localhost', '127.0.0.1'].includes(window.location.hostname);
const defaultApiBaseUrl = isLocalFrontend ? 'http://localhost:8080' : CLOUD_API_BASE_URL;
const savedApiBaseUrl = localStorage.getItem('API_BASE_URL');
const isSavedLocalhostUrl = savedApiBaseUrl && /^https?:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/i.test(savedApiBaseUrl);
const isSavedPreviousEc2Url = savedApiBaseUrl && /^https?:\/\/18\.222\.191\.97(?::8080)?\/?$/i.test(savedApiBaseUrl);
const shouldUseDefaultUrl = !savedApiBaseUrl || (!isLocalFrontend && (isSavedLocalhostUrl || isSavedPreviousEc2Url));

let API_BASE_URL = shouldUseDefaultUrl ? defaultApiBaseUrl : savedApiBaseUrl;

if (savedApiBaseUrl !== API_BASE_URL) {
  localStorage.setItem('API_BASE_URL', API_BASE_URL);
}

const client = axios.create({
  baseURL: API_BASE_URL,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Actualizar baseURL dinámicamente
export const setApiBaseUrl = (url) => {
  API_BASE_URL = url.replace(/\/$/, '');
  localStorage.setItem('API_BASE_URL', API_BASE_URL);
  client.defaults.baseURL = API_BASE_URL;
};

export const getApiBaseUrl = () => API_BASE_URL;

// API Endpoints
export const apiService = {
  // 1. POST /api/execute
  executeCommands: async (commands) => {
    try {
      const response = await client.post('/api/execute', { commands });
      return response.data;
    } catch (error) {
      if (error.response) {
        return error.response.data;
      }
      throw new Error('Error de conexión con el servidor backend (' + API_BASE_URL + ')');
    }
  },

  // 2. GET /api/file
  getFileUrl: (path) => {
    if (!path) return '';
    return `${API_BASE_URL}/api/file?path=${encodeURIComponent(path)}`;
  },

  getFileContent: async (path) => {
    try {
      const response = await client.get(`/api/file?path=${encodeURIComponent(path)}`, {
        responseType: 'text',
      });
      return response.data;
    } catch (error) {
      throw new Error(error.response?.data?.message || 'No se pudo cargar el archivo');
    }
  },

  // 3. POST /api/login
  login: async (user, pass, id) => {
    try {
      const response = await client.post('/api/login', { user, pass, id });
      return response.data;
    } catch (error) {
      if (error.response) {
        return error.response.data;
      }
      throw new Error('No se pudo conectar con el backend para iniciar sesión');
    }
  },

  // 4. POST /api/logout
  logout: async () => {
    try {
      const response = await client.post('/api/logout');
      return response.data;
    } catch (error) {
      if (error.response) return error.response.data;
      return { status: 'success', message: 'Sesión cerrada localmente' };
    }
  },

  // 5. GET /api/session
  getSession: async () => {
    try {
      const response = await client.get('/api/session');
      return response.data;
    } catch (error) {
      return { isLoggedIn: false, user: '', id: '' };
    }
  },

  // 6. GET /api/partitions
  getPartitions: async (disk = '') => {
    try {
      const url = disk ? `/api/partitions?disk=${encodeURIComponent(disk)}` : '/api/partitions';
      const response = await client.get(url);
      return response.data;
    } catch (error) {
      return { status: 'error', partitions: [], message: error.message };
    }
  },

  // 7. GET /api/disks
  getDisks: async (path = '') => {
    try {
      const url = path ? `/api/disks?path=${encodeURIComponent(path)}` : '/api/disks';
      const response = await client.get(url);
      return response.data;
    } catch (error) {
      return { status: 'error', disks: [], message: error.message };
    }
  },

  // 8. GET /api/journal
  getJournal: async (id = '') => {
    try {
      const url = id ? `/api/journal?id=${encodeURIComponent(id)}` : '/api/journal';
      const response = await client.get(url);
      return response.data;
    } catch (error) {
      if (error.response) return error.response.data;
      return { status: 'error', journaling: [], message: error.message };
    }
  },

  // 9. GET /api/tree
  getTree: async (id = '', path = '', output = '') => {
    try {
      let url = `/api/tree?id=${encodeURIComponent(id)}`;
      if (path) url += `&path=${encodeURIComponent(path)}`;
      if (output) url += `&output=${encodeURIComponent(output)}`;
      const response = await client.get(url);
      return response.data;
    } catch (error) {
      if (error.response) return error.response.data;
      return { status: 'error', message: error.message };
    }
  }
};
