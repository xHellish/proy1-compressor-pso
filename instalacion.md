# Compresor PSO

## Requisitos del sistema

Este proyecto está pensado para ejecutarse en un Debian 13 recién instalado, con la instalación por defecto del escritorio GNOME y sin herramientas extra de desarrollo instaladas previamente.

Se requiere:

- Debian 13
- entorno gráfico GNOME activo
- acceso de usuario con permisos de sudo
- `gcc`
- `make`
- `pkg-config`
- GTK 4
- OpenSSL (para MD5)
- libcurl (para el downloader de Gutenberg)

## Preinstalación para Debian 13 de fábrica

En una Debian 13 de escritorio con GNOME por defecto, ejecuta esto en una terminal:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  pkg-config \
  libgtk-4-dev \
  libglib2.0-dev \
  libssl-dev \
  libcurl4-openssl-dev
```

Si por alguna razón no están instalados estos paquetes base, también puedes hacerlo por separado:

```bash
sudo apt install -y build-essential pkg-config libgtk-4-dev libglib2.0-dev libssl-dev libcurl4-openssl-dev
```

Si el sistema no tiene herramientas de compilación:

```bash
sudo apt install -y gcc make g++ git
```

## Verificar que todo quedó instalado

```bash
gcc --version
make --version
pkg-config --modversion gtk4
```

Si el último comando devuelve una versión de GTK 4, la instalación está correcta.

> En Debian 13 de fábrica, con estas dependencias instaladas, el proyecto compila sin necesidad de paquetes extra.

## Compilar el proyecto

Desde la carpeta del proyecto:

```bash
cd /home/benfarz/Escritorio/Compressor\ PSO/proy1-compressor-pso
make
```

Esto genera los binarios:

- `main` (interfaz gráfica)
- `compresor_normal`
- `compresor_fork`
- `compresor_pthread`
- `descompresor_normal`
- `descompresor_fork`
- `descompresor_pthread`

## Ejecutar la interfaz gráfica

```bash
./main
```

La interfaz te permite:

- seleccionar una carpeta de origen,
- seleccionar una carpeta de destino,
- escoger un archivo comprimido,
- ejecutar compresión y descompresión por cada método,
- ver resultados comparativos en una tabla.

## Ejecutar los módulos por línea de comandos

### Compresión serial

```bash
./compresor_normal <directorio_entrada> <directorio_salida>
```

### Descompresión serial

```bash
./descompresor_normal <archivo_comprimido.huff> <directorio_destino>
```

### Compresión con fork

```bash
./compresor_fork <directorio_entrada> <directorio_salida>
```

### Descompresión con fork

```bash
./descompresor_fork <archivo_comprimido.huff> <directorio_destino>
```

### Compresión con pthread

```bash
./compresor_pthread <directorio_entrada> <directorio_salida>
```

### Descompresión con pthread

```bash
./descompresor_pthread <archivo_comprimido.huff> <directorio_destino>
```

## Descargar libros de Gutenberg

Si quieres usar el descargador incluido del top 100 de Gutenberg:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic gutenberg_downloader.c -o gutenberg_downloader -lcurl
./gutenberg_downloader
```

Esto crea una carpeta con archivos `.txt` en formato de texto plano.

## Nota importante sobre el MD5

El proyecto usa OpenSSL para calcular MD5 y verificar la integridad del archivo después de descomprimir.

Por eso es obligatorio tener instalado `libssl-dev`.

## Limpieza del proyecto

```bash
make clean
```

## Solución de problemas comunes

### Error: `gtk/gtk.h: No such file or directory`

Instala GTK 4:

```bash
sudo apt install -y libgtk-4-dev
```

### Error: `openssl/md5.h: No such file or directory`

Instala OpenSSL dev:

```bash
sudo apt install -y libssl-dev
```

### Error: `curl/curl.h: No such file or directory`

Instala libcurl:

```bash
sudo apt install -y libcurl4-openssl-dev
```

### Error al abrir la GUI

Asegúrate de estar en un entorno gráfico de GNOME o de escritorio con GTK habilitado.

## Resumen rápido

```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-4-dev libglib2.0-dev libssl-dev libcurl4-openssl-dev
cd /home/benfarz/Escritorio/Compressor\ PSO/proy1-compressor-pso
make
./main
```

Con eso, el proyecto debería poder compilar y ejecutarse correctamente en Debian 13 con entorno gráfico GNOME.
