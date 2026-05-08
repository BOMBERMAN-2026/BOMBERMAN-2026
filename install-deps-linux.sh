#!/usr/bin/env bash

set -euo pipefail

if [[ $(id -u) -ne 0 ]]; then
    echo "Este script debe ejecutarse con sudo o como root."
    echo "Ejemplo: sudo ./install-deps-linux.sh"
    exit 1
fi

if command -v apt-get >/dev/null 2>&1; then
    echo "Instalando dependencias para Debian/Ubuntu..."
    apt-get update
    apt-get install -y \
        build-essential \
        cmake \
        pkg-config \
        libglew-dev \
        libglfw3-dev \
        libgl1-mesa-dev \
        libx11-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libxxf86vm-dev \
        libavformat-dev \
        libavcodec-dev \
        libavutil-dev \
        libswscale-dev \
        libssl-dev
    echo "Dependencias instaladas." \
         "Ahora puedes generar build con cmake y compilar el proyecto." 
else
    echo "No se detectó apt-get."
    echo "Este script está diseñado para Debian/Ubuntu."
    echo "Instala manualmente los paquetes equivalentes en tu distribución." 
    exit 2
fi
