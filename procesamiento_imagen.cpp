#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono> // Agrega esto al inicio del archivo
#include <omp.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

using namespace std;

// Estructura básica para las plantillas
struct TileTemplate {
    string name;
    vector<unsigned char> data;
    int w, h, c;
    int negros, cafes, blancos, otros; // histogramas preprocesados
};

struct ColorRange {
    int r_min, r_max;
    int g_min, g_max;
    int b_min, b_max;
};

int tomar_captura();

// Funciones de carga de imágenes
unsigned char* cargar_imagen(const string& filename, int& width, int& height, int& channels) {
    unsigned char* img = stbi_load(filename.c_str(), &width, &height, &channels, 3);
    if (!img) {
        cerr << "No se pudo cargar la imagen: " << filename << endl;
    }
    return img;
}

// Nueva función para leer los histogramas preprocesados
vector<TileTemplate> cargar_plantillas_preprocesadas(const string& archivo, int cantidad) {
    vector<TileTemplate> templates;
    ifstream fin(archivo);
    if (!fin) {
        cerr << "No se pudo abrir " << archivo << endl;
        return templates;
    }
    for (int i = 0; i < cantidad; ++i) {
        string fname;
        int w, h, n, caf, bla, o;
        if (!(fin >> fname >> w >> h >> n >> caf >> bla >> o)) break;
        int c = 3;
        // Si necesitas la imagen para comparar píxel a píxel:
        int tw, th, tc;
        unsigned char* tdata = cargar_imagen(fname, tw, th, tc);
        vector<unsigned char> data;
        if (tdata) {
            data.assign(tdata, tdata + tw * th * 3);
            stbi_image_free(tdata);
        }
        templates.push_back({fname, data, w, h, c, n, caf, bla, o});
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

int es_personaje(const vector<unsigned char>& cell) {
    bool blanco = false;
    bool beige = false;
    bool boton = false;
    bool llave = false;
    for (size_t i = 0; i < cell.size(); i += 3) {
        unsigned char r = cell[i], g = cell[i+1], b = cell[i+2];
        if (r == 230 && g == 249 && b == 255) blanco = true;
        else if (r == 206 && g == 182 && b == 146) beige = true;
        else if (r == 117 && g == 80 && b == 61) boton = true;
        else if (r == 20 && g == 121 && b == 90) llave = true;
        if (blanco && beige && boton) return 19; // Personaje con botón
        if (blanco && llave) return 15; // Personaje con llave
        if (blanco && beige) return 4; // Si ambos colores están presentes, es un personaje
    }
    return -1; 
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

vector<int> calcular_histograma_rgb(const vector<unsigned char>& img, int bins_per_channel = 4) {
    int total_bins = bins_per_channel * bins_per_channel * bins_per_channel;
    vector<int> hist(total_bins, 0);
    int step = 256 / bins_per_channel;
    for (size_t i = 0; i < img.size(); i += 3) {
        int r_bin = img[i] / step;
        int g_bin = img[i+1] / step;
        int b_bin = img[i+2] / step;
        int idx = r_bin * bins_per_channel * bins_per_channel + g_bin * bins_per_channel + b_bin;
        hist[idx]++;
    }
    return hist;
}

void normalizar_histograma(std::vector<int>& hist) {
    int total = 0;
    for (int v : hist) total += v;
    if (total == 0) return;
    for (int& v : hist) v = double(v) / total * 1000; // Escala para mantener precisión entera
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

double chi2_hist(const std::vector<int>& h1, const std::vector<int>& h2) {
    double chi2 = 0.0;
    const double eps = 1e-10;
    for (size_t i = 0; i < h1.size(); ++i) {
        double num = h1[i] - h2[i];
        double denom = h1[i] + h2[i] + eps;
        chi2 += (num * num) / denom;
    }
    return chi2;
}

vector<unsigned char> extraer_region(const vector<unsigned char>& img, int w, int h, int x0, int y0, int x1, int y1) {
    vector<unsigned char> region;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            int idx = (y * w + x) * 3;
            region.push_back(img[idx]);
            region.push_back(img[idx + 1]);
            region.push_back(img[idx + 2]);
        }
    }
    return region;
}

// Clasificación de celdas
vector<vector<int>> clasificar_celdas(const unsigned char* img, int width, int height,
                                    int filas, int columnas, int block_w, int block_h,
                                    const vector<TileTemplate>& templates) {
    vector<vector<int>> etiquetas(filas, vector<int>(columnas, -1));
    unordered_map<int, int> tile_to_tipo = {
        {3, 1}, {4, 1}, {11, 1}, {12, 1}, {13, 1}, {14, 1}, {15, 1}, {16, 1}, {39, 1},  // pared
        {7, 0}, {30, 0}, {32, 0}, {38, 0}, {40, 0}, {41, 0}, {42, 0}, {44, 0}, // piso
        {0, 2},  // diamante
        {2, 3},  // llave
        {5, 4},  // personaje
        {8, 5},  // puerta
        {6, 6},  // piedra
        {9, 7}, {25, 7}, {17, 7}, //pinchos
        {10, 8}, // salida
        {1, 9}, {43, 9}, // hueco
        {18, 10}, {19, 10}, {20, 10}, {21, 10}, {22, 10}, {23, 10}, {24, 10},  // lava
        {26, 11},  // reja
        {27, 12},  // boton
        {28, 13},  // estatua
        {29, 14},  // pinchos - afuera
        {31, 15},  // personaje con llave
        {32, 16},  // piedra en hueco
        {33, 17},  // reja abajo
        {34, 18},  // piedra en boton
        {35, 19},  // personaje en boton
        {36, 20},  // Piedra en pinchos
        {37, 21},  // Personaje - Boton - Llaves
 
    };

    // --- Fase 1: Detectar bloque de encima del personaje
    vector<vector<bool>> bloque_bloqueado(filas, vector<bool>(columnas, false));
    for (int i = filas - 1; i >= 0; --i) {
        for (int j = 0; j < columnas; ++j) {
            vector<unsigned char> cell = extraer_celda(img, width, height, block_w, block_h, i, j);
            if (es_personaje(cell) && i > 0) {
                bloque_bloqueado[i-1][j] = true;
            }
        }
    }

    // --- Fase 2: Clasificación paralelizada ---
    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int i = 0; i < filas; ++i) {
        for (int j = 0; j < columnas; ++j) {

            // Las dos primeras filas siempre son pared
            if (i < 2) {
                etiquetas[i][j] = 1;
                continue;
            }

            vector<unsigned char> cell = extraer_celda(img, width, height, block_w, block_h, i, j);

            
            bool tiene_color_diamante = false;
            bool tiene_color_llave = false;
            bool tiene_color_pared = false;
            bool tiene_color_salida = false;
            bool tiene_gris = false;
            bool tiene_cafe = false;
            bool color_piso = false;
            for (size_t idx = 0; idx < cell.size(); idx += 3) {
                unsigned char r = cell[idx], g = cell[idx+1], b = cell[idx+2];

                if ((r == 67 || r == 66) && (g == 76 || g == 78) && (b  == 63 || b == 64)) {
                    tiene_color_diamante = true;
                    break;
                }
                // if (r == 57 && g == 237 && b == 218) {
                //     tiene_color_llave = true;
                //     break;
                // }
                if ((r == 63 && g == 40 && b == 28)) {
                    color_piso = true;
                }
                if (r == 38 && g == 38 && b == 38) {
                    tiene_color_pared = true;
                }
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

            if (tiene_color_diamante) {
                etiquetas[i][j] = 2;
                continue;
            }
            // if (tiene_color_llave) {
            //     etiquetas[i][j] = 3;
            //     continue;
            // }
            if (tiene_color_pared) {
                if (color_piso){
                    etiquetas[i][j] = 0;
                    continue;
                }
                etiquetas[i][j] = 1;
                continue;
            }
            // if (tiene_color_salida) {
            //     etiquetas[i][j] = 8;
            //     continue;
            // }
            int personaje = es_personaje(cell);
            if (personaje > 0) {
                etiquetas[i][j] = personaje;
                //continue;
            }

            // --- Aquí usa bloque_bloqueado[i][j] ---
            bool es_bloque_bloqueado = bloque_bloqueado[i][j];
            
            const double REGION_CENTRAL = 1; // 50% del tamaño original

            double min_diff = numeric_limits<double>::max();
            int best_idx = -1;
            for (size_t t = 0; t < templates.size(); ++t) {
                int w = templates[t].w, h = templates[t].h;
                // Región central
                int region_w = int(w * REGION_CENTRAL);  // 50% del ancho
                int region_h = int(h * REGION_CENTRAL);  // 50% de la altura
                int x0 = (w - region_w) / 4;  // Centrado en X
                int y0 = (h - region_h) / 4;  // Centrado en Y
                int x1 = x0 + region_w;
                int y1 = y0 + region_h;

                double diff = 0;
                if (es_bloque_bloqueado) {
                    double mae = calc_mae(cell, templates[t].data, w, h, 0, 0, w);

                    // Región central del 50%
                    int region_w = w * 0.6;
                    int region_h = h * 0.3;
                    int x0_c = (w - region_w) / 2;
                    int y0_c = (h - region_h) / 2;
                    int x1_c = x0_c + region_w;
                    int y1_c = y0_c + region_h;

                    double diff_central = calc_mae(cell, templates[t].data, x1_c, y1_c, x0_c, y0_c, w);

                    int n1, c1, b1, o1, n2, c2, b2, o2;
                    histograma(cell, n1, c1, b1, o1);
                    histograma(templates[t].data, n2, c2, b2, o2);
                    double diff_hist_simple = abs(n1-n2) + abs(c1-c2) + abs(b1-b2) + abs(o1-o2);

                    // Histograma de la región central del 50%
                    vector<unsigned char> region_cell = extraer_region(cell, w, h, x0_c, y0_c, x1_c, y1_c);
                    vector<unsigned char> region_template = extraer_region(templates[t].data, w, h, x0_c, y0_c, x1_c, y1_c);

                    vector<int> hist_cell_full = calcular_histograma_rgb(region_cell, 4);
                    vector<int> hist_template_full = calcular_histograma_rgb(region_template, 4);
                    normalizar_histograma(hist_cell_full);
                    normalizar_histograma(hist_template_full);
                    double diff_hist = chi2_hist(hist_cell_full, hist_template_full);

                    double peso_hist = 1;
                    double peso_mae = 1.0;
                    double peso_central = 1.0;
                    diff = peso_hist * diff_hist;

                } else {
                    // --- Región central: histograma + chi2 ---
                    vector<unsigned char> region_cell = extraer_region(cell, w, h, x0, y0, x1, y1);
                    vector<unsigned char> region_template = extraer_region(templates[t].data, w, h, x0, y0, x1, y1);

                    vector<int> hist_central_cell = calcular_histograma_rgb(region_cell, 4);
                    vector<int> hist_central_template = calcular_histograma_rgb(region_template, 4);
                            
                    //normalizar_histograma(hist_central_cell);
                    //normalizar_histograma(hist_central_template);
                    double diff_central = chi2_hist(hist_central_cell, hist_central_template);

                    double peso_hist = 0;
                    double peso_central = 1;
                    diff = peso_central * diff_central;
                    //diff = peso_hist * diff_hist;
                }
                if (diff < min_diff) {
                    min_diff = diff;
                    best_idx = t;
                }
            }
            etiquetas[i][j] = tile_to_tipo.count(best_idx) ? tile_to_tipo[best_idx] : best_idx;

            // Si la etiqueta es piedra (6, 18, 20), compara solo con plantillas 6, 34, 36
            if (etiquetas[i][j] == 6 || etiquetas[i][j] == 18 || etiquetas[i][j] == 20) {
                // Extraer región central del 50%
                int region_w = block_w * 0.8;
                int region_h = block_h - 15;
                int x0 = (block_w - region_w) / 2;
                int y0 = (block_h - region_h) / 2;
                int x1 = x0 + region_w;
                int y1 = y0 + region_h;
                vector<unsigned char> region_central = extraer_region(cell, block_w, block_h, x0, y0, x1, y1);

                bool hay_negro = false, hay_cafe = false;
                for (size_t idx = 0; idx < region_central.size(); idx += 3) {
                    unsigned char r = region_central[idx], g = region_central[idx+1], b = region_central[idx+2];
                    if (r == 16 && g == 9 && b == 5)
                        hay_negro = true;
                    else if (r == 117 && g == 80 && b == 61)
                        hay_cafe = true;
                }
                if (hay_cafe && hay_negro) {
                    etiquetas[i][j] = 18; // Piedra en botón
                } else if (hay_negro) {
                    etiquetas[i][j] = 20; // Piedra en pinchos
                } else {
                    etiquetas[i][j] = 6; // Piedra normal
                }
            }

            // Si la etiqueta es personaje (4, 19), compara solo con plantillas 5, 35
            if (etiquetas[i][j] == 4 || etiquetas[i][j] == 19) {
                // Extraer región central del 50%
                int region_w = block_w;
                int region_h = block_h;
                int x0 = (block_w - region_w) / 2;
                int y0 = (block_h - region_h) / 2;
                int x1 = x0 + region_w;
                int y1 = y0 + region_h;
                vector<unsigned char> region_central = extraer_region(cell, block_w, block_h, x0, y0, x1, y1);

                // Cuenta la cantidad de negros en la celda
                int negros_celda = 0;
                for (size_t idx = 0; idx < cell.size(); idx += 3) {
                    unsigned char r = cell[idx], g = cell[idx+1], b = cell[idx+2];
                    if (r < 40 && g < 40 && b < 40)
                        negros_celda++;
                }

                // Compara con las plantillas 5 y 35 (ajusta si tus índices son otros)
                int idx_5 = 5, idx_35 = 35;
                int negros_5 = templates[idx_5].negros;
                int negros_35 = templates[idx_35].negros;

                int diff_5 = abs(negros_celda - negros_5);
                int diff_35 = abs(negros_celda - negros_35);

                // Asigna la etiqueta según la plantilla más cercana en cantidad de negros
                if (diff_5 < diff_35)
                    etiquetas[i][j] = 4; // Personaje normal
                else
                    etiquetas[i][j] = 19; // Personaje en botón
            }

            if (etiquetas[i][j] == 14 || etiquetas[i][j] == 17) {
                vector<int> candidatos = {29, 33};
                double min_diff = std::numeric_limits<double>::max();
                int mejor_etiqueta = etiquetas[i][j];

                for (int cand : candidatos) {
                    if (cand < 0 || cand >= (int)templates.size()) continue;
                    int w = block_w, h = block_h;
                    int region_w = w;
                    int region_h = h;
                    int x0 = (w - region_w) / 2;
                    int y0 = (h - region_h) / 2;
                    int x1 = x0 + region_w;
                    int y1 = y0 + region_h;

                    // Extrae la región central del 80% del bloque y la plantilla
                    vector<unsigned char> region_cell = extraer_region(cell, w, h, x0, y0, x1, y1);
                    vector<unsigned char> region_template = extraer_region(templates[cand].data, w, h, x0, y0, x1, y1);

                    vector<int> hist_cell = calcular_histograma_rgb(region_cell, 4);
                    vector<int> hist_template = calcular_histograma_rgb(region_template, 4);
                    normalizar_histograma(hist_cell);
                    normalizar_histograma(hist_template);
                    double diff = chi2_hist(hist_cell, hist_template);
                    if (diff < min_diff) {
                        min_diff = diff;
                        mejor_etiqueta = tile_to_tipo.count(cand) ? tile_to_tipo[cand] : cand;
                    }
                }
                etiquetas[i][j] = mejor_etiqueta;
            }
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

void guardar_matriz_txt(const vector<vector<int>>& etiquetas, const string& filename) {
    ofstream fout(filename);
    if (!fout) {
        cerr << "No se pudo abrir " << filename << " para escritura." << endl;
        return;
    }
    for (const auto& fila : etiquetas) {
        for (size_t j = 0; j < fila.size(); ++j) {
            fout << fila[j];
            if (j + 1 < fila.size()) fout << " ";
        }
        fout << "\n";
    }
    fout.close();
}


int main() {

    using namespace chrono;
    auto start = high_resolution_clock::now(); // Marca el inicio

    tomar_captura();


    int width, height, channels;
    int filas = 15;
    int columnas = 10;
    int cant_tiles = 45;
    int cont_matri_confl = 0;

    unsigned char* img = cargar_imagen("captura_firefox.png", width, height, channels);

    if (!img) {
        cerr << "No se pudo cargar " << img << endl;
        return 1;
    }

    int block_h = round((float)height / filas);
    int block_w = round((float)width / columnas);
    
    vector<TileTemplate> templates = cargar_plantillas_preprocesadas("plantillas_preprocesadas.txt", cant_tiles);
    vector<vector<int>> etiquetas = clasificar_celdas(img, width, height, filas, columnas, block_w, block_h, templates);
    guardar_matriz_txt(etiquetas, "matriz_clasificacion.txt");
    system("python3 solver.py");


    //imprimir_matriz(etiquetas);
    stbi_image_free(img);

    

    // Leer matrices de referencia
    // vector<vector<vector<int>>> matrices_ref = leer_matrices_archivo("extras/niveles.txt", filas, columnas);

    // int idx_ref = 0;
    // for (int nivel = 2; nivel <= 20; ++nivel, ++idx_ref) {
    //     string fname = "niveles/nivel" + to_string(nivel) + ".png";
    //     unsigned char* img = cargar_imagen(fname, width, height, channels);
    //     if (!img) {
    //         cerr << "No se pudo cargar " << fname << endl;
    //         continue;
    //     }

    //     int block_h = round((float)height / filas);
    //     int block_w = round((float)width / columnas);

    //     vector<TileTemplate> templates = cargar_plantillas_preprocesadas("plantillas_preprocesadas.txt", cant_tiles);
    //     vector<vector<int>> etiquetas = clasificar_celdas(img, width, height, filas, columnas, block_w, block_h, templates);

    //     // Comparar con la matriz de referencia
    //     if (idx_ref >= matrices_ref.size() || !matrices_iguales(etiquetas, matrices_ref[idx_ref])) {
    //         cont_matri_confl++;
    //         cout << "Diferencia en nivel " << nivel << ":\n";
    //         cout << "Generada:\n";
    //         imprimir_matriz(etiquetas);
    //         cout << "Referencia:\n";
    //         if (idx_ref < matrices_ref.size())
    //             imprimir_matriz(matrices_ref[idx_ref]);
    //         else
    //             cout << "(No hay matriz de referencia)\n";

    //         // Mostrar cantidad y posiciones de casillas diferentes
    //         int diferencias = 0;
    //         if (idx_ref < matrices_ref.size()) {
    //             for (int i = 0; i < filas; ++i) {
    //                 for (int j = 0; j < columnas; ++j) {
    //                     if (etiquetas[i][j] != matrices_ref[idx_ref][i][j]) {
    //                         diferencias++;
    //                         cout << "Diferencia en (" << i << ", " << j << "): "
    //                              << "Generada=" << etiquetas[i][j]
    //                              << ", Referencia=" << matrices_ref[idx_ref][i][j] << endl;

    //                     }
    //                 }
    //             }
    //         }
    //         cout << "Cantidad de casillas diferentes: " << diferencias << endl;
    //         cout << endl;
    //     }

    //     stbi_image_free(img);
    // }

    // cout << "Total de matrices con conflicto: " << cont_matri_confl << endl;

    auto end = high_resolution_clock::now(); // Marca el final
    auto duration = duration_cast<milliseconds>(end - start);
    cout << "Tiempo de ejecución Total Programa: " << duration.count() << " ms" << endl;

    return 0;
}