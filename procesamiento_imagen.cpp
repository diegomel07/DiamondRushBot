#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

struct TileTemplate {
    string name;
    vector<unsigned char> data;
    int w, h, c;
};

// Carga una imagen y retorna el puntero y sus dimensiones
unsigned char* cargar_imagen(const string& filename, int& width, int& height, int& channels) {
    unsigned char* img = stbi_load(filename.c_str(), &width, &height, &channels, 3);
    if (!img) {
        cerr << "No se pudo cargar la imagen: " << filename << endl;
    }
    return img;
}

// Extrae una celda de la imagen principal
vector<unsigned char> extraer_celda(const unsigned char* img, int img_w, int img_h, int block_w, int block_h, int i, int j) {
    vector<unsigned char> cell(block_w * block_h * 3);
    for (int y = 0; y < block_h; ++y) {
        for (int x = 0; x < block_w; ++x) {
            int src_idx = ((i * block_h + y) * img_w + (j * block_w + x)) * 3;
            int dst_idx = (y * block_w + x) * 3;
            for (int k = 0; k < 3; ++k) {
                cell[dst_idx + k] = img[src_idx + k];
            }
        }
    }
    return cell;
}

// Calcula la diferencia absoluta total entre una celda y una plantilla
double diff_template(const unsigned char* cell, int cw, int ch, const TileTemplate& tile) {
    double diff = 0;
    for (int y = 0; y < ch; ++y) {
        for (int x = 0; x < cw; ++x) {
            int idx_cell = (y * cw + x) * 3;
            int idx_tile = (y * tile.w + x) * 3;
            for (int k = 0; k < 3; ++k) {
                diff += abs(int(cell[idx_cell + k]) - int(tile.data[idx_tile + k]));
            }
        }
    }
    return diff;
}

// Carga todas las plantillas de la carpeta tiles/
vector<TileTemplate> cargar_plantillas(int block_w, int block_h, int cantidad) {
    vector<TileTemplate> templates;
    for (int i = 0; i < cantidad; ++i) {
        string fname = "tiles/tile" + to_string(i) + ".png";
        int tw, th, tc;
        unsigned char* tdata = stbi_load(fname.c_str(), &tw, &th, &tc, 3);
        if (!tdata) {
            cerr << "No se pudo cargar la plantilla: " << fname << endl;
            continue;
        }
        if (tw != block_w || th != block_h) {
            cerr << "Tamaño de plantilla no coincide con la celda: " << fname << endl;
            stbi_image_free(tdata);
            continue;
        }
        templates.push_back({fname, vector<unsigned char>(tdata, tdata + tw * th * 3), tw, th, 3});
        stbi_image_free(tdata);
    }
    return templates;
}

// Realiza el template matching y retorna la matriz de etiquetas
vector<vector<int>> clasificar_celdas(const unsigned char* img, int width, int height, int filas, int columnas, int block_w, int block_h, const vector<TileTemplate>& templates) {
    vector<vector<int>> etiquetas(filas, vector<int>(columnas, -1));
    for (int i = 0; i < filas; ++i) {
        for (int j = 0; j < columnas; ++j) {
            vector<unsigned char> cell = extraer_celda(img, width, height, block_w, block_h, i, j);
            double min_diff = std::numeric_limits<double>::max();
            int best_idx = -1;
            for (size_t t = 0; t < templates.size(); ++t) {
                double d = diff_template(cell.data(), block_w, block_h, templates[t]);
                if (d < min_diff) {
                    min_diff = d;
                    best_idx = t;
                }
            }
            etiquetas[i][j] = best_idx;
        }
    }
    return etiquetas;
}

// Imprime la matriz de etiquetas
void imprimir_matriz(const vector<vector<int>>& etiquetas) {
    // Empieza desde la fila 3 (índice 3)
    for (size_t i = 3; i < etiquetas.size(); ++i) {
        for (int val : etiquetas[i]) {
            cout << val << " ";
        }
        cout << endl;
    }
}

int main() {
    int width, height, channels;
    unsigned char* img = cargar_imagen("captura_firefox.png", width, height, channels);
    if (!img) return 1;

    // Parámetros de la cuadrícula
    int filas = 15;
    int columnas = 10;
    int block_h = round((float)height / filas);
    int block_w = round((float)width / columnas);
    int cant_tiles = 13;

    // Cargar plantillas
    vector<TileTemplate> templates = cargar_plantillas(block_w, block_h, cant_tiles);

    // Clasificar celdas
    vector<vector<int>> etiquetas = clasificar_celdas(img, width, height, filas, columnas, block_w, block_h, templates);

    // Imprimir resultado
    imprimir_matriz(etiquetas);

    stbi_image_free(img);
    return 0;
}

// 0 - diamante
// 1 - hueco
// 2 - llave
// 3 - pared
// 4 - pared
// 5 - personaje
// 6 - piedra
// 7 - suelo
// 8 - puera
// 9 - pinchos
// 10 - salida
// 11 - lava