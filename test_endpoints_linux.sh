#!/bin/bash
# Script de pruebas automáticas con curl para Linux (Ubuntu/Debian)

SERVER_URL="http://localhost:8080"
echo -e "\033[1;36m====================================================\033[0m"
echo -e "\033[1;32m      Pruebas de Endpoints HTTP en Linux\033[0m"
echo -e "\033[1;36m====================================================\033[0m"

echo -e "\n\033[1;33m[Prueba 1] GET /api/session\033[0m"
curl -s -X GET "$SERVER_URL/api/session" | grep -o '.*'

echo -e "\n\n\033[1;33m[Prueba 2] POST /api/execute (Crear disco de prueba, partición y montar)\033[0m"
PAYLOAD='{"commands":"mkdisk -size=10 -u=m -path=./disco_linux.dsk\nfdisk -type=P -u=k -name=PartLinux -size=1000 -path=./disco_linux.dsk -f=BF\nmount -path=./disco_linux.dsk -name=PartLinux"}'
curl -s -X POST "$SERVER_URL/api/execute" -H "Content-Type: application/json" -d "$PAYLOAD"

echo -e "\n\n\033[1;33m[Prueba 3] GET /api/partitions\033[0m"
curl -s -X GET "$SERVER_URL/api/partitions"

echo -e "\n\n\033[1;33m[Prueba 4] GET /api/disks\033[0m"
curl -s -X GET "$SERVER_URL/api/disks"

echo -e "\n\n\033[1;33m[Prueba 5] POST /api/execute (Formatear EXT3 con mkfs)\033[0m"
# Obtener primer id montado o usar por defecto
PAYLOAD_MKFS='{"commands":"mkfs -type=fast -id=761A -fs=3fs"}'
curl -s -X POST "$SERVER_URL/api/execute" -H "Content-Type: application/json" -d "$PAYLOAD_MKFS"

echo -e "\n\n\033[1;33m[Prueba 6] POST /api/login\033[0m"
curl -s -X POST "$SERVER_URL/api/login" -H "Content-Type: application/json" -d '{"user":"root","pass":"123","id":"761A"}'

echo -e "\n\n\033[1;33m[Prueba 7] GET /api/journal?id=761A\033[0m"
curl -s -X GET "$SERVER_URL/api/journal?id=761A"

echo -e "\n\n\033[1;33m[Prueba 8] GET /api/tree?id=761A\033[0m"
curl -s -X GET "$SERVER_URL/api/tree?id=761A"

echo -e "\n\n\033[1;33m[Prueba 9] OPTIONS /api/execute (CORS Preflight)\033[0m"
curl -s -I -X OPTIONS "$SERVER_URL/api/execute" -H "Origin: http://localhost:5173" -H "Access-Control-Request-Method: POST" | grep -i "Access-Control"

echo -e "\n\n\033[1;33m[Prueba 10] POST /api/logout\033[0m"
curl -s -X POST "$SERVER_URL/api/logout"

echo -e "\n\n\033[1;32m¡Pruebas finalizadas con éxito!\033[0m"
