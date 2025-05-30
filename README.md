# Bot para el juego Diamond Rush

Este proyecto es un bot diseñado para analizar y resolver niveles del juego **Diamond Rush** utilizando captura de pantalla y procesamiento de imágenes.

## Compilación

Ejecuta los siguientes comandos en una terminal para compilar los archivos fuente:

```bash

make

make run

```

## Dependencia libx11-dev y X11

Es necesario tener la dependencia libx11-dev instalada para que el código de captura de pantalla funcione correctamente, ya que este bot utiliza X11 para interactuar con la interfaz gráfica.

Para instalar libx11-dev en la mayoría de las distribuciones de Linux, puedes usar los siguientes comandos:
En sistemas basados en Debian/Ubuntu:

```bash
sudo apt update
sudo apt install libx11-dev
```
