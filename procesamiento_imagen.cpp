#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <iomanip>
#include <fstream>
#include <sstream>

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
        unsigned char* tdata = cargar_imagen(fname, tw, th, tc);
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
    return (r >= 40 && r <= 90) && (g >= 25 && g <= 65) && (b >= 20 && b <= 50);
}

bool es_tile_todo_cafe(const vector<unsigned char>& cell) {
    int total = cell.size() / 3;
    int cafe = 0;
    for (int i = 0; i < total; ++i) {
        if (es_color_cafe(cell[i*3], cell[i*3+1], cell[i*3+2]))
            cafe++;
    }
    // Considera "todo café" si más del 85% de los píxeles lo son
    return cafe > total * 0.85;
}

bool es_personaje(const vector<unsigned char>& cell) {
    bool blanco = false;
    bool beige = false;
    for (size_t i = 0; i < cell.size(); i += 3) {
        unsigned char r = cell[i], g = cell[i+1], b = cell[i+2];
        if (r == 236 && g == 254 && b == 255) blanco = true;
        else if (r == 206 && g == 182 && b == 146) beige = true;
        if (blanco && beige) return true; // Si ambos colores están presentes, es un personaje
    }
    return false;
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

double calc_mae(const vector<unsigned char>& cell, const vector<unsigned char>& templ, int x1, int y1, int x0, int y0, int w) {
    double mae = 0;
    int cuenta = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            int idx = (y * w + x) * 3;
            for (int k = 0; k < 3; ++k)
                mae += abs(int(cell[idx + k]) - int(templ[idx + k]));
            cuenta++;
        }
    return cuenta > 0 ? mae / (cuenta * 3.0) : 1e9;
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

    // Variables para guardar la posición a bloquear
    int bloquear_i = -1, bloquear_j = -1;

    for (int i = filas - 1; i >= 0; --i) {
        for (int j = 0; j < columnas; ++j) {

            // Las dos primeras filas siempre son pared
            if (i < 2) {
                etiquetas[i][j] = 1;
                continue;
            }

            vector<unsigned char> cell = extraer_celda(img, width, height, block_w, block_h, i, j);

            // Si la celda es un diamante
            bool tiene_color_diamante = false;
            for (size_t idx = 0; idx < cell.size(); idx += 3) {
                unsigned char r = cell[idx], g = cell[idx+1], b = cell[idx+2];
                if ((r == 67 || r == 66) && (g == 76 || g == 78) && (b  == 63 || b == 64)) {
                    tiene_color_diamante = true;
                    break;
                }
            }

            if (tiene_color_diamante) {
                etiquetas[i][j] = 2;
                continue;
            }

            // si la celda es una llave
            bool tiene_color_llave = false;
            for (size_t idx = 0; idx < cell.size(); idx += 3) {
                unsigned char r = cell[idx], g = cell[idx+1], b = cell[idx+2];
                if (r == 57 && g == 237 && b == 218) {
                    tiene_color_llave = true;
                    break;
                }
            }

            if (tiene_color_llave) {
                etiquetas[i][j] = 3;
                continue;
            }


            // si tiene el color del menu (gris) es una pared, pero si hay color de piso, es piso
            bool tiene_color_pared = false;
            bool color_piso = false;
            for (size_t idx = 0; idx < cell.size(); idx += 3) {
                unsigned char r = cell[idx], g = cell[idx+1], b = cell[idx+2];
                if ((r == 63 && g == 40 && b == 28)) {
                    color_piso = true;
                }
                if (r == 38 && g == 38 && b == 38) {
                    tiene_color_pared = true;
                }
            }

            if (tiene_color_pared) {
                if (color_piso){
                    etiquetas[i][j] = 0;
                    continue;
                }
                etiquetas[i][j] = 1;
                continue;
            }
            

            // Si es un bloque de salida
            bool tiene_color_salida = false;
            bool tiene_gris = false;
            bool tiene_cafe = false;

            for (size_t idx = 0; idx < cell.size(); idx += 3) {
                unsigned char r = cell[idx], g = cell[idx+1], b = cell[idx+2];
                if ((r <= 162 && r >= 155) && (g >= 150 && g <= 160) && (b >= 150 && b <= 160)) {
                    tiene_gris = true;
                }
                else if (((r >= 95 && r <=  106) && (g >= 45 && g <= 55) && (b >= 15 && b <= 24))){
                    tiene_cafe = true;
                }
                if (tiene_cafe && tiene_gris) {
                    tiene_color_salida = true;
                    break;
                }
                
            }

            if (tiene_color_salida) {
                etiquetas[i][j] = 8;
                continue;
            }

            // Si es personaje, guarda la posición del bloque de arriba
            if (es_personaje(cell)) {
                etiquetas[i][j] = 4;
                if (i > 0) {
                    bloquear_i = i - 1;
                    bloquear_j = j;
                }
                continue;
            }

            
            // Revision de los demas boques
            double min_diff = numeric_limits<double>::max();
            int best_idx = -1;
            for (size_t t = 0; t < templates.size(); ++t) {
                int w = templates[t].w, h = templates[t].h;
                int x0 = w / 4, x1 = 3 * w / 4;
                int y0 = h / 4, y1 = 3 * h / 4;

                bool es_bloque_bloqueado = (i == bloquear_i && j == bloquear_j);
                double diff = 0;
                
                // bloque de encima del personaje
                if (es_bloque_bloqueado) {

                    // --- Error medio absoluto en todo el bloque ---
                    double mae = calc_mae(cell, templates[t].data, w, h, 0, 0, w);
                    

                    // --- Error medio en la región central ---
                    double diff_central = calc_mae(cell, templates[t].data, x1, y1, x0, y0, w);
                    
                    // --- Histograma de color ---
                    int n1, c1, b1, o1, n2, c2, b2, o2;
                    histograma(cell, n1, c1, b1, o1);
                    histograma(templates[t].data, n2, c2, b2, o2);
                    double diff_hist = abs(n1-n2) + abs(c1-c2) + abs(b1-b2) + abs(o1-o2);


                    // Valor de cada componente de los errores
                    double peso_hist = 0.035;
                    double peso_mae = 1.0;
                    double peso_central = 2.0;
                    diff = peso_mae * mae + peso_central * diff_central + peso_hist * diff_hist;

                } else {
                    // --- Lógica para los demas bloques ---

                    // --- Error medio en la region central ---
                    double diff_central = calc_mae(cell, templates[t].data, x1, y1, x0, y0, w);

                    // --- Histograma de color ---
                    int n1, c1, b1, o1, n2, c2, b2, o2;
                    histograma(cell, n1, c1, b1, o1);
                    histograma(templates[t].data, n2, c2, b2, o2);
                    double diff_hist = abs(n1-n2) + abs(c1-c2) + abs(b1-b2) + abs(o1-o2);

                    double peso_hist = 0.079;
                    double peso_central = 3;
                    diff = peso_hist * diff_hist + peso_central * diff_central;
                }

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

// Función para leer todas las matrices del archivo
vector<vector<vector<int>>> leer_matrices_archivo(const string& filename, int filas, int columnas) {
    vector<vector<vector<int>>> matrices;
    ifstream fin(filename);
    if (!fin) {
        cerr << "No se pudo abrir el archivo de referencia." << endl;
        return matrices;
    }
    string line;
    while (getline(fin, line)) {
        if (line.find("Nivel") != string::npos) {
            vector<vector<int>> matriz;
            for (int i = 0; i < filas; ++i) {
                if (!getline(fin, line)) break;
                istringstream iss(line);
                vector<int> fila;
                int val;
                for (int j = 0; j < columnas; ++j) {
                    iss >> val;
                    fila.push_back(val);
                }
                matriz.push_back(fila);
            }
            matrices.push_back(matriz);
        }
    }
    return matrices;
}

// Función para comparar dos matrices
bool matrices_iguales(const vector<vector<int>>& a, const vector<vector<int>>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

// Función para imprimir una matriz
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
    int filas = 15;
    int columnas = 10;
    int cant_tiles = 29;
    int cont_matri_confl = 0;


    // unsigned char* img = cargar_imagen("captura_firefox.png", width, height, channels);
    // if (!img) {
    //     cerr << "No se pudo cargar " << img << endl;
    //     return 1;
    // }

    // int block_h = round((float)height / filas);
    // int block_w = round((float)width / columnas);

    // vector<TileTemplate> templates = cargar_plantillas(block_w, block_h, cant_tiles);
    // vector<vector<int>> etiquetas = clasificar_celdas(img, width, height, filas, columnas, block_w, block_h, templates);
    
    // imprimir_matriz(etiquetas);
    // stbi_image_free(img);

    // Leer matrices de referencia
    vector<vector<vector<int>>> matrices_ref = leer_matrices_archivo("niveles.txt", filas, columnas);

    int idx_ref = 0;
    for (int nivel = 2; nivel <= 20; ++nivel, ++idx_ref) {
        string fname = "niveles/nivel" + to_string(nivel) + ".png";
        unsigned char* img = cargar_imagen(fname, width, height, channels);
        if (!img) {
            cerr << "No se pudo cargar " << fname << endl;
            continue;
        }

        int block_h = round((float)height / filas);
        int block_w = round((float)width / columnas);

        vector<TileTemplate> templates = cargar_plantillas(block_w, block_h, cant_tiles);
        vector<vector<int>> etiquetas = clasificar_celdas(img, width, height, filas, columnas, block_w, block_h, templates);

        // Comparar con la matriz de referencia
        if (idx_ref >= matrices_ref.size() || !matrices_iguales(etiquetas, matrices_ref[idx_ref])) {
            cont_matri_confl++;
            cout << "Diferencia en nivel " << nivel << ":\n";
            cout << "Generada:\n";
            imprimir_matriz(etiquetas);
            cout << "Referencia:\n";
            if (idx_ref < matrices_ref.size())
                imprimir_matriz(matrices_ref[idx_ref]);
            else
                cout << "(No hay matriz de referencia)\n";
            cout << endl;
        }

        stbi_image_free(img);
    }

    cout << "Total de matrices con conflicto: " << cont_matri_confl << endl;

    return 0;
}

