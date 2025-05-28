#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <iomanip>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

// Estructura básica para las plantillas
struct TileTemplate {
    string name;
    vector<unsigned char> data;
    int w, h, c;
};

// Funciones de carga de imágenes
unsigned char* cargar_imagen(const string& filename, int& width, int& height, int& channels) {
    unsigned char* img = stbi_load(filename.c_str(), &width, &height, &channels, 3);
    if (!img) {
        cerr << "No se pudo cargar la imagen: " << filename << endl;
    }
    return img;
}

vector<TileTemplate> cargar_plantillas(int block_w, int block_h, int cantidad) {
    vector<TileTemplate> templates;
    for (int i = 0; i < cantidad; ++i) {
        string fname = "tiles/tile" + to_string(i) + ".png";
        int tw, th, tc;
        unsigned char* tdata = stbi_load(fname.c_str(), &tw, &th, &tc, 3);
        if (!tdata || tw != block_w || th != block_h) {
            if (tdata) stbi_image_free(tdata);
            continue;
        }
        templates.push_back({fname, vector<unsigned char>(tdata, tdata + tw * th * 3), tw, th, 3});
        stbi_image_free(tdata);
    }
    return templates;
}

// Extrae una celda de la imagen
vector<unsigned char> extraer_celda(const unsigned char* img, int img_w, int img_h, 
                                  int block_w, int block_h, int i, int j) {
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

// Devuelve true si el color está dentro del rango de café
bool es_color_cafe(unsigned char r, unsigned char g, unsigned char b) {
    // Ajusta estos valores según tu fondo
    return (r >= 40 && r <= 90) && (g >= 25 && g <= 65) && (b >= 20 && b <= 50);
}

bool es_tile_todo_cafe(const vector<unsigned char>& cell) {
    int total = cell.size() / 3;
    int cafe = 0;
    for (int i = 0; i < total; ++i) {
        if (es_color_cafe(cell[i*3], cell[i*3+1], cell[i*3+2]))
            cafe++;
    }
    // Considera "todo café" si más del 95% de los píxeles lo son
    return cafe > total * 0.85;
}

// --- Utilidades para características ---
vector<pair<int, int>> puntos_clave(int w, int h) {
    return {
        {w/4, h/4}, {w/2, h/4}, {3*w/4, h/4},
        {w/4, h/2}, {w/2, h/2}, {3*w/4, h/2},
        {w/4, 3*h/4}, {w/2, 3*h/4}, {3*w/4, 3*h/4}
    };
}

void histograma(const vector<unsigned char>& img, int& negros, int& cafes, int& blancos, int& otros) {
    negros = cafes = blancos = otros = 0;
    for (size_t i = 0; i < img.size(); i += 3) {
        unsigned char r = img[i], g = img[i+1], b = img[i+2];
        if (r < 40 && g < 40 && b < 40)
            negros++;
        else if (es_color_cafe(r, g, b))
            cafes++;
        else if (r > 220 && g > 220 && b > 220) // blanco puro o casi blanco
            blancos++;
        else
            otros++;
    }
}

// --- Comparación combinada ---
double comparar_celda_template(const vector<unsigned char>& cell, const TileTemplate& templ, bool solo_cuarto_superior_de_mitad_abajo = false) {
    int w = templ.w, h = templ.h;

    // 2. Histograma de color
    int n1, c1, b1, o1, n2, c2, b2, o2;
    histograma(cell, n1, c1, b1, o1);
    histograma(templ.data, n2, c2, b2, o2);
    double diff_hist = abs(n1-n2) + abs(c1-c2) + abs(b1-b2) + abs(o1-o2);

    // 3. Región especial para fila 3
    double diff_central = 0;
    int cuenta = 0;
    int x0 = w / 4, x1 = 3 * w / 4;
    int y0 = h / 4, y1 = 3 * h / 4;
    if (solo_cuarto_superior_de_mitad_abajo) {
        y0 = h / 2;         // 50% del alto
        y1 = h * 3 / 4;     // 75% del alto
    }
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            int idx = (y * w + x) * 3;
            for (int k = 0; k < 3; ++k)
                diff_central += abs(int(cell[idx + k]) - int(templ.data[idx + k]));
            cuenta++;
        }
    diff_central /= (cuenta * 3.0);

    double peso_hist = 0.079;
    double peso_central = 3;

    return peso_hist * diff_hist +
           peso_central * diff_central;
}

// Clasificación de celdas
vector<vector<int>> clasificar_celdas(const unsigned char* img, int width, int height,
                                    int filas, int columnas, int block_w, int block_h,
                                    const vector<TileTemplate>& templates) {
    vector<vector<int>> etiquetas(filas, vector<int>(columnas, -1));
    
    unordered_map<int, int> tile_to_tipo = {
        {3, 1}, {4, 1}, {11, 1}, {12, 1}, {13, 1}, {14, 1}, {15, 1}, {16, 1}, // pared 
        {7, 0},  // piso
        {0, 2},  // diamante
        {2, 3},  // llave
        {5, 4},  // personaje
        {8, 5},  // puerta
        {6, 6},  // piedra
        {9, 7}, {25, 7}, {17, 7}, // pinchos
        {10, 8}, // salida
        {1, 9},  // hueco
        {18, 10}, {19, 10}, {20, 10}, {21, 10}, {22, 10}, {23, 10}, {24, 10}, // lava
        {26, 11}, // reja
        {27, 12},  // boton
        {28, 13}  // estatua
    };

    for (int i = 0; i < filas; ++i) {
        for (int j = 0; j < columnas; ++j) {
            if (i < 2) {
                // Ignora las dos primeras filas y pon pared
                etiquetas[i][j] = 1;
                continue;
            }
            vector<unsigned char> cell = extraer_celda(img, width, height, block_w, block_h, i, j);

            double min_diff = numeric_limits<double>::max();
            int best_idx = -1;

            for (size_t t = 0; t < templates.size(); ++t) {
                // Solo el cuarto superior de la mitad inferior en la fila 3 (i == 2)
                bool solo_cuarto_superior_de_mitad_abajo = (i == 2);
                double diff = comparar_celda_template(cell, templates[t], solo_cuarto_superior_de_mitad_abajo);
                if (diff < min_diff) {
                    min_diff = diff;
                    best_idx = t;
                }
            }
            etiquetas[i][j] = tile_to_tipo.count(best_idx) ? tile_to_tipo[best_idx] : best_idx;
        }
    }
    
    return etiquetas;
}

// Función de impresión
void imprimir_matriz(const vector<vector<int>>& etiquetas) {
    for (const auto& fila : etiquetas) {
        for (int val : fila) {
            cout << setw(3) << val << " ";
        }
        cout << endl;
    }
}

int main() {
    int width, height, channels;
    unsigned char* img = cargar_imagen("niveles/nivel20.png", width, height, channels);
    if (!img) return 1;

    int filas = 15;
    int columnas = 10;
    int block_h = round((float)height / filas);
    int block_w = round((float)width / columnas);
    int cant_tiles = 29;

    vector<TileTemplate> templates = cargar_plantillas(block_w, block_h, cant_tiles);
    vector<vector<int>> etiquetas = clasificar_celdas(img, width, height, filas, columnas, 
                                                     block_w, block_h, templates);
    imprimir_matriz(etiquetas);

    stbi_image_free(img);
    return 0;
}


// fallos: la puerta la confunde con lava y el personaje aveces el tile de arribe se vuelve 7