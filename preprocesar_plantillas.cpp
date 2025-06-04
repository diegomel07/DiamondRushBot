#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

// Devuelve true si el color está dentro del rango de café
bool es_color_cafe(unsigned char r, unsigned char g, unsigned char b) {
    return (r >= 40 && r <= 90) && (g >= 25 && g <= 65) && (b >= 20 && b <= 50);
}

void histograma(const vector<unsigned char>& img, int& negros, int& cafes, int& blancos, int& otros) {
    negros = cafes = blancos = otros = 0;
    for (size_t i = 0; i < img.size(); i += 3) {
        unsigned char r = img[i], g = img[i+1], b = img[i+2];
        if (r < 40 && g < 40 && b < 40)
            negros++;
        else if (es_color_cafe(r, g, b))
            cafes++;
        else
            otros++;
    }
}

int main() {
    int cant_tiles = 45;
    ofstream fout("plantillas_preprocesadas.txt");
    if (!fout) {
        cerr << "No se pudo abrir el archivo de salida." << endl;
        return 1;
    }

    for (int i = 0; i < cant_tiles; ++i) {
        string fname = "tiles/tile" + to_string(i) + ".png";
        int w, h, c;
        unsigned char* data = stbi_load(fname.c_str(), &w, &h, &c, 3);
        if (!data) {
            cerr << "No se pudo cargar " << fname << endl;
            continue;
        }
        vector<unsigned char> img(data, data + w * h * 3);
        int n, caf, bla, o;
        histograma(img, n, caf, bla, o);
        fout << fname << " " << w << " " << h << " " << n << " " << caf << " " << bla << " " << o << "\n";
        stbi_image_free(data);
    }
    fout.close();
    cout << "Preprocesamiento terminado. Archivo: plantillas_preprocesadas.txt" << endl;
    return 0;
}