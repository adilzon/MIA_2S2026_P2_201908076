#!/bin/bash
# Script para compilar y ejecutar el backend C++ en Linux (Ubuntu/Debian)

echo -e "\033[1;36m====================================================\033[0m"
echo -e "\033[1;32m      Iniciando Backend C++ (Linux) - MIA 2S2026\033[0m"
echo -e "\033[1;36m====================================================\033[0m"

# Navegar a la carpeta fuente
cd "$(dirname "$0")/proyecto_1/src" || exit 1

# Compilar el servidor HTTP
echo -e "\033[1;33m[1/2] Compilando api_server con make...\033[0m"
make api

if [ $? -ne 0 ]; then
    echo -e "\033[1;31m[ERROR] Falló la compilación del servidor.\033[0m"
    exit 1
fi

# Dar permisos de ejecución
chmod +x ./api_server

# Ejecutar el servidor en puerto 8080
echo -e "\033[1;32m[2/2] Ejecutando ./api_server en puerto 8080...\033[0m"
./api_server
