#!/bin/bash
# Script para instalar dependencias y levantar el frontend React + Vite en Linux

echo -e "\033[1;36m====================================================\033[0m"
echo -e "\033[1;32m   Iniciando Frontend React + Vite (Linux)\033[0m"
echo -e "\033[1;36m====================================================\033[0m"

cd "$(dirname "$0")/frontend" || exit 1

if [ ! -d "node_modules" ]; then
    echo -e "\033[1;33m[1/2] Instalando módulos npm...\033[0m"
    npm install
fi

echo -e "\033[1;32m[2/2] Iniciando Vite dev server en http://localhost:5173...\033[0m"
npm run dev
