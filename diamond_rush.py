import itertools
import heapq
from dataclasses import dataclass

def cargar_nivel(ruta_archivo: str, indice_nivel: int = 0):
    with open(ruta_archivo, 'r') as f:
        contenido = f.read().strip().splitlines()

    niveles = []
    nivel_actual = []
    for linea in contenido:
        linea = linea.strip()
        if not linea:
            if nivel_actual:
                niveles.append(nivel_actual)
                nivel_actual = []
        elif not any(c.isdigit() for c in linea):
            continue
        else:
            try:
                fila = [int(x) for x in linea.split()]
                nivel_actual.append(fila)
            except ValueError:
                continue

    if nivel_actual:
        niveles.append(nivel_actual)

    matriz = niveles[indice_nivel]
    filas = len(matriz)
    cols = len(matriz[0]) if filas > 0 else 0

    jugador = None
    diamantes = []
    piedras = []
    pinchos = []
    huecos = []
    salida = None

    for i in range(filas):
        for j in range(cols):
            celda = matriz[i][j]
            if celda == 4:
                jugador = (i, j)
            elif celda == 2:
                diamantes.append((i, j))
            elif celda == 6:
                piedras.append((i, j))
            elif celda == 7:
                pinchos.append((i, j))
            elif celda == 9:
                huecos.append((i, j))
            elif celda == 8:
                salida = (i, j)

    if salida is None:
        raise ValueError("Nivel sin salida especificada")

    estado_inicial = GameState(
        player=jugador,
        remaining_diamonds=frozenset(diamantes),
        stone_positions=frozenset(piedras),
        used_spikes=frozenset(),
        filled_holes=frozenset(),
        has_key=False,
        opened_doors=frozenset()
    )
    return matriz, estado_inicial, salida

@dataclass(frozen=True)
class GameState:
    player: tuple[int, int]
    remaining_diamonds: frozenset[tuple[int, int]]
    stone_positions: frozenset[tuple[int, int]]
    used_spikes: frozenset[tuple[int, int]]
    filled_holes: frozenset[tuple[int, int]]
    has_key: bool
    opened_doors: frozenset[tuple[int, int]]

    def __hash__(self):
        return hash((self.player, self.remaining_diamonds, 
                     self.stone_positions, self.used_spikes, 
                     self.filled_holes, self.has_key, self.opened_doors))

MOVES = {
    (-1, 0): "arriba",
    (1, 0): "abajo",
    (0, -1): "izquierda",
    (0, 1): "derecha"
}

def generar_sucesores(estado: GameState, matriz, salida_coord):
    sucesores = []
    filas = len(matriz)
    cols = len(matriz[0]) if filas > 0 else 0
    px, py = estado.player

    piedra_en_boton = any(matriz[x][y] == 12 for (x, y) in estado.stone_positions)

    def es_transitable(x, y, tiene_llave, puertas_abiertas):
        if not (0 <= x < filas and 0 <= y < cols):
            return False
        valor = matriz[x][y]
        if (x, y) in estado.stone_positions:
            return False
        if valor in {1, 10, 13}:
            return False
        if valor == 11 and not piedra_en_boton:
            return False
        if valor == 5:
            if (x, y) in puertas_abiertas:
                return True
            return tiene_llave
        if valor == 9 and (x, y) not in estado.filled_holes:
            return False
        if valor == 7 and (x, y) in estado.used_spikes:
            return False
        return True

    for (dx, dy), accion in MOVES.items():
        nx, ny = px + dx, py + dy
        if 0 <= nx < filas and 0 <= ny < cols:
            celda = matriz[nx][ny]
            if (nx, ny) in estado.stone_positions:
                bx, by = nx + dx, ny + dy
                if es_transitable(bx, by, estado.has_key, estado.opened_doors) and matriz[bx][by] not in {5, 8}:
                    nuevas_piedras = set(estado.stone_positions)
                    nuevas_piedras.remove((nx, ny))
                    nuevos_huecos = set(estado.filled_holes)
                    if matriz[bx][by] == 9:
                        nuevos_huecos.add((bx, by))
                    else:
                        nuevas_piedras.add((bx, by))
                    nuevos_pinchos_usados = set(estado.used_spikes)
                    if matriz[bx][by] == 7:
                        nuevos_pinchos_usados.add((bx, by))
                    if matriz[nx][ny] == 7:
                        nuevos_pinchos_usados.add((nx, ny))
                    nuevo_estado = GameState(
                        player=(nx, ny),
                        remaining_diamonds=estado.remaining_diamonds,
                        stone_positions=frozenset(nuevas_piedras),
                        used_spikes=frozenset(nuevos_pinchos_usados),
                        filled_holes=frozenset(nuevos_huecos),
                        has_key=estado.has_key,
                        opened_doors=estado.opened_doors
                    )
                    sucesores.append((nuevo_estado, accion))
            else:
                if es_transitable(nx, ny, estado.has_key, estado.opened_doors):
                    nuevos_diamantes = estado.remaining_diamonds
                    if (nx, ny) in estado.remaining_diamonds:
                        nuevos_diamantes = set(nuevos_diamantes)
                        nuevos_diamantes.remove((nx, ny))
                        nuevos_diamantes = frozenset(nuevos_diamantes)

                    nuevos_pinchos_usados = set(estado.used_spikes)
                    if matriz[nx][ny] == 7:
                        nuevos_pinchos_usados.add((nx, ny))

                    nueva_llave = estado.has_key
                    nuevas_puertas = set(estado.opened_doors)

                    if matriz[nx][ny] == 3 and not estado.has_key:
                        nueva_llave = True
                    elif matriz[nx][ny] == 5:
                        if (nx, ny) not in estado.opened_doors and estado.has_key:
                            nuevas_puertas.add((nx, ny))
                            nueva_llave = False

                    nuevo_estado = GameState(
                        player=(nx, ny),
                        remaining_diamonds=nuevos_diamantes,
                        stone_positions=estado.stone_positions,
                        used_spikes=frozenset(nuevos_pinchos_usados),
                        filled_holes=estado.filled_holes,
                        has_key=nueva_llave,
                        opened_doors=frozenset(nuevas_puertas)
                    )
                    sucesores.append((nuevo_estado, accion))
    return sucesores

def heuristica(estado: GameState, salida_coord):
    if len(estado.remaining_diamonds) == 0:
        px, py = estado.player
        sx, sy = salida_coord
        return abs(px - sx) + abs(py - sy)
    puntos = [estado.player] + list(estado.remaining_diamonds) + [salida_coord]
    n = len(puntos)
    no_visitados = set(range(n))
    no_visitados.remove(0)
    visitados = {0}
    dist_min = [float('inf')] * n
    px, py = puntos[0]
    for j in no_visitados:
        x, y = puntos[j]
        dist_min[j] = abs(px - x) + abs(py - y)
    mst_cost = 0
    while no_visitados:
        j_mejor = min(no_visitados, key=lambda j: dist_min[j])
        mst_cost += dist_min[j_mejor]
        no_visitados.remove(j_mejor)
        visitados.add(j_mejor)
        xj, yj = puntos[j_mejor]
        for k in no_visitados:
            xk, yk = puntos[k]
            dist = abs(xj - xk) + abs(yj - yk)
            if dist < dist_min[k]:
                dist_min[k] = dist
    return mst_cost

def encontrar_camino_optimo(matriz, estado_inicial: GameState, salida_coord):
    frontera = []
    contador = itertools.count()
    heapq.heappush(frontera, (heuristica(estado_inicial, salida_coord), 0, next(contador), estado_inicial))
    mejor_coste = {estado_inicial: 0}
    predecesor = {estado_inicial: (None, None)}

    while frontera:
        f, g, _, estado = heapq.heappop(frontera)
        if len(estado.remaining_diamonds) == 0 and estado.player == salida_coord:
            acciones = []
            actual = estado
            while predecesor[actual][0] is not None:
                anterior, accion = predecesor[actual]
                acciones.append(accion)
                actual = anterior
            acciones.reverse()
            return acciones
        for nuevo_estado, accion in generar_sucesores(estado, matriz, salida_coord):
            nuevo_g = g + 1
            if nuevo_estado not in mejor_coste or nuevo_g < mejor_coste[nuevo_estado]:
                mejor_coste[nuevo_estado] = nuevo_g
                predecesor[nuevo_estado] = (estado, accion)
                f_nuevo = nuevo_g + heuristica(nuevo_estado, salida_coord)
                heapq.heappush(frontera, (f_nuevo, nuevo_g, next(contador), nuevo_estado))
    return None

if __name__ == "__main__":
    ruta = "extras/niveles.txt"
    nivel = 2
    matriz, estado_inicial, salida = cargar_nivel(ruta, nivel)
    solucion = encontrar_camino_optimo(matriz, estado_inicial, salida)
    print(solucion)
