import axios from 'axios';

let API_BASE_URL = localStorage.getItem('API_BASE_URL') || 'http://localhost:8080';

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
  getPartitions: async () => {
    try {
      const response = await client.get('/api/partitions');
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
  getTree: async (id = '', output = '') => {
    try {
      let url = `/api/tree?id=${encodeURIComponent(id)}`;
      if (output) url += `&output=${encodeURIComponent(output)}`;
      const response = await client.get(url);
      return response.data;
    } catch (error) {
      if (error.response) return error.response.data;
      return { status: 'error', message: error.message };
    }
  }
};
