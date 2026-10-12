#!/bin/bash
# Script para compilar y ejecutar el backend C++ en Linux (Ubuntu/Debian)

echo -e "\033[1;36m====================================================\033[0m"
echo -e "\033[1;32m      Iniciando Backend C++ (Linux) - MIA 2S2026\033[0m"
echo -e "\033[1;36m====================================================\033[0m"

# Navegar a la carpeta fuente
cd "$(dirname "$0")/proyecto_1/src" || exit 1

# Verificar herramientas requeridas
if ! command -v g++ &> /dev/null || ! command -v make &> /dev/null; then
    echo -e "\033[1;31m[ERROR] g++ o make no están instalados.\033[0m"
    echo -e "Ejecuta: \033[1;33msudo apt update && sudo apt install -y build-essential graphviz\033[0m"
    exit 1
fi

# 1. Limpieza de binarios antiguos / objetos compilados en Windows
echo -e "\033[1;33m[1/3] Limpiando binarios previos con make clean...\033[0m"
make clean

# 2. Compilar el servidor HTTP para Linux
echo -e "\033[1;33m[2/3] Compilando api_server con make api...\033[0m"
make api

if [ $? -ne 0 ]; then
    echo -e "\033[1;31m[ERROR] Falló la compilación del servidor.\033[0m"
    exit 1
fi

# 3. Dar permisos y ejecutar
chmod +x ./api_server
echo -e "\033[1;32m[3/3] Ejecutando ./api_server en http://localhost:8080...\033[0m"
./api_server
