#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <iostream>
#include <string>
#include <chrono> // Agrega esto al inicio

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

// Busca la ventana de Firefox por nombre
Window FindWindowByName(Display* display, Window root, const string& name) {
    Window parent;
    Window *children;
    unsigned int nchildren;
    Window result = 0;

    if (XQueryTree(display, root, &root, &parent, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; ++i) {
            XTextProperty prop;
            if (XGetWMName(display, children[i], &prop) && prop.value) {
                string winName = (char*)prop.value;
                if (winName.find(name) != string::npos) {
                    result = children[i];
                    XFree(prop.value);
                    break;
                }
                XFree(prop.value);
            }
            // Recursivo: busca en hijos
            result = FindWindowByName(display, children[i], name);
            if (result) break;
        }
        if (children) XFree(children);
    }
    return result;
}

void ImageFromWindowRegion(
    vector<uint8_t>& Pixels,
    int& Width, int& Height, int& BitsPerPixel,
    Window win, Display* display,
    int offset_x, int offset_y, int region_width, int region_height)
{
    XWindowAttributes attributes = {0};
    XGetWindowAttributes(display, win, &attributes);

    // Limita la región a los límites de la ventana
    if (offset_x < 0) offset_x = 0;
    if (offset_y < 0) offset_y = 0;
    if (offset_x + region_width > attributes.width)
        region_width = attributes.width - offset_x;
    if (offset_y + region_height > attributes.height)
        region_height = attributes.height - offset_y;

    Width = region_width;
    Height = region_height;

    XImage* img = XGetImage(display, win, offset_x, offset_y, Width, Height, AllPlanes, ZPixmap);
    if (!img) {
        cerr << "Error al obtener la imagen de la ventana.\n";
        return;
    }
    BitsPerPixel = img->bits_per_pixel;
    Pixels.resize(Width * Height * 4);

    // Convertir a RGBA
    for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x) {
            unsigned long pixel = XGetPixel(img, x, y);
            uint8_t r = (pixel & img->red_mask) >> 16;
            uint8_t g = (pixel & img->green_mask) >> 8;
            uint8_t b = (pixel & img->blue_mask);
            Pixels[4 * (y * Width + x) + 0] = r;
            Pixels[4 * (y * Width + x) + 1] = g;
            Pixels[4 * (y * Width + x) + 2] = b;
            Pixels[4 * (y * Width + x) + 3] = 255;
        }
    }

    XDestroyImage(img);
}

int tomar_captura()
{

    int Width, Height, BitsPerPixel;
    vector<uint8_t> Pixels;

    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        cerr << "No se pudo abrir el display X11.\n";
        return 1;
    }

    Window root = DefaultRootWindow(display);
    Window firefoxWin = FindWindowByName(display, root, "Firefox");
    if (!firefoxWin) {
        cerr << "No se encontró una ventana de Firefox.\n";
        XCloseDisplay(display);
        return 1;
    }

    cout << "Ventana de Firefox encontrada: " << firefoxWin << "\n";    
    int canvas_offset_x = 666;
    int canvas_offset_y = 276;
    int canvas_width = 427;
    int canvas_height = 640;

    ImageFromWindowRegion(Pixels, Width, Height, BitsPerPixel, firefoxWin, display, canvas_offset_x, canvas_offset_y, canvas_width, canvas_height);

    if (stbi_write_png("captura_firefox.png", Width, Height, 4, Pixels.data(), Width * 4)) {
        cout << "Captura de ventana de Firefox guardada como 'captura_firefox.png'\n";
    } else {
        cerr << "Error al guardar la captura\n";
    }

    XCloseDisplay(display);
    return 0;
}