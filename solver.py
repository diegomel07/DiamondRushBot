import heapq
from itertools import count
import pyautogui
import time

class Estado:
    __slots__ = (
        "jugador", "diamantes", "llaves_pos", "keys", "puertas", "pinchos_activados",
        "rocas", "huecos_rellenos", "botones",
        "parent", "accion_from_parent"
    )

    def __init__(self, jugador, diamantes, llaves_pos, keys, puertas, pinchos_activados,
                 rocas, huecos_rellenos, botones=None,
                 parent=None, accion_from_parent=None):
        self.jugador = jugador
        self.diamantes = frozenset(diamantes)
        self.llaves_pos = frozenset(llaves_pos)
        self.keys = keys
        self.puertas = frozenset(puertas)
        self.pinchos_activados = frozenset(pinchos_activados)
        self.rocas = frozenset(rocas)
        self.huecos_rellenos = frozenset(huecos_rellenos)
        if botones is None:
            botones = {}
        self.botones = tuple(sorted(botones.items()))
        self.parent = parent
        self.accion_from_parent = accion_from_parent

    def __eq__(self, other):
        return (
            isinstance(other, Estado)
            and self.jugador == other.jugador
            and self.diamantes == other.diamantes
            and self.llaves_pos == other.llaves_pos
            and self.keys == other.keys
            and self.puertas == other.puertas
            and self.pinchos_activados == other.pinchos_activados
            and self.rocas == other.rocas
            and self.huecos_rellenos == other.huecos_rellenos
            and self.botones == other.botones
        )

    def __hash__(self):
        return hash((
            self.jugador,
            self.diamantes,
            self.llaves_pos,
            self.keys,
            self.puertas,
            self.pinchos_activados,
            self.rocas,
            self.huecos_rellenos,
            self.botones
        ))

    def es_meta(self, coord_salida):
        return (not self.diamantes) and (self.jugador == coord_salida)

    def reconstruir_ruta(self):
        acciones = []
        est = self
        while est is not None and est.accion_from_parent is not None:
            acciones.append(est.accion_from_parent)
            est = est.parent
        acciones.reverse()
        return acciones

def parse_level(ruta_archivo):
    with open(ruta_archivo, "r", encoding="utf-8") as f:
        lineas = [l.strip() for l in f if l.strip()]
    mapa = []
    for linea in lineas:
        fila = [int(tok) for tok in linea.split()]
        mapa.append(fila)
    alto = len(mapa)
    ancho = len(mapa[0]) if alto > 0 else 0
    pos_jugador = None
    diamantes = set()
    llaves = set()
    puertas = set()
    pinchos = set()
    rocas = set()
    huecos = set()
    lava = set()
    rejas = set()
    botones = {}
    estatuas = {}
    coord_salida = None
    for r in range(alto):
        for c in range(ancho):
            codigo = mapa[r][c]
            if codigo == 4:
                pos_jugador = (r, c)
                mapa[r][c] = 0
            elif codigo == 2:
                diamantes.add((r, c))
            elif codigo == 3:
                llaves.add((r, c))
            elif codigo == 5:
                puertas.add((r, c))
            elif codigo == 6:
                rocas.add((r, c))
            elif codigo == 7:
                pinchos.add((r, c))
            elif codigo == 8:
                coord_salida = (r, c)
                mapa[r][c] = 8
            elif codigo == 9:
                huecos.add((r, c))
            elif codigo == 10:
                lava.add((r, c))
            elif codigo == 11:
                rejas.add((r, c))
            elif codigo == 12:
                botones[(r, c)] = False
            elif codigo == 13:
                estatuas[(r, c)] = False
    estado_inicial = Estado(
        jugador=pos_jugador,
        diamantes=diamantes,
        llaves_pos=llaves,
        keys=0,
        puertas=puertas,
        pinchos_activados=set(),
        rocas=rocas,
        huecos_rellenos=set(),
        botones={**botones, **estatuas},
        parent=None,
        accion_from_parent=None
    )
    return mapa, estado_inicial, coord_salida

def aplicar_mecanismo(botones_dict, puertas_set, rejas_set, r, c):
    botones_dict[(r, c)] = True
    pass

def vecinos(estado, mapa, coord_salida):
    sucesores = []
    filas = len(mapa)
    cols = len(mapa[0])
    r0, c0 = estado.jugador
    diamantes = set(estado.diamantes)
    llaves_pos = set(estado.llaves_pos)
    keys = estado.keys
    puertas = set(estado.puertas)
    pinchos_activados = set(estado.pinchos_activados)
    rocas = set(estado.rocas)
    huecos_rellenos = set(estado.huecos_rellenos)
    botones_items = dict(estado.botones)
    movimientos = [
        (-1, 0, "arriba"),
        (1,  0, "abajo"),
        (0, -1, "izquierda"),
        (0,  1, "derecha"),
    ]
    for dr, dc, accion in movimientos:
        rn = r0 + dr
        cn = c0 + dc
        if not (0 <= rn < filas and 0 <= cn < cols):
            continue
        tile = mapa[rn][cn]
        if tile == 1 or tile == 11:
            continue
        if (rn, cn) in puertas and keys == 0:
            continue
        if (rn, cn) in rocas:
            rs = rn + dr
            cs = cn + dc
            if not (0 <= rs < filas and 0 <= cs < cols):
                continue
            tile_siguiente = mapa[rs][cs]
            if tile_siguiente == 1 or tile_siguiente == 11:
                continue
            if (rs, cs) in rocas:
                continue
            if (rs, cs) in puertas and keys == 0:
                continue
            if (rs, cs) in puertas:
                continue
            nuevas_rocas = set(rocas)
            nuevas_rocas.remove((rn, cn))
            nuevos_huecos_rellenos = set(huecos_rellenos)
            nuevas_diamantes = set(diamantes)
            nuevas_llaves_pos = set(llaves_pos)
            nuevas_llaves = keys
            nuevos_puertas = set(puertas)
            nuevos_pinchos_activados = set(pinchos_activados)
            nuevos_botones = dict(botones_items)
            if tile_siguiente == 9 and (rs, cs) not in huecos_rellenos:
                nuevos_huecos_rellenos.add((rs, cs))
            elif tile_siguiente == 10 and (rs, cs) not in huecos_rellenos:
                pass
            else:
                nuevas_rocas.add((rs, cs))
            rj, cj = rn, cn
            if (rj, cj) in nuevas_diamantes:
                nuevas_diamantes.remove((rj, cj))
            if tile == 3 and (rj, cj) in nuevas_llaves_pos:
                if nuevas_llaves == 0:
                    nuevas_llaves = 1
                    nuevas_llaves_pos.remove((rj, cj))
            if (rj, cj) in puertas and nuevas_llaves > 0:
                nuevos_puertas.remove((rj, cj))
                nuevas_llaves = 0
            if tile == 7 and (rj, cj) not in pinchos_activados:
                nuevos_pinchos_activados.add((rj, cj))
            elif tile == 7:
                continue
            if tile == 9 and (rj, cj) not in huecos_rellenos:
                continue
            if tile == 10 and (rj, cj) not in huecos_rellenos:
                continue
            if tile == 8 and nuevas_diamantes:
                continue  # No ir a la salida si quedan diamantes
            if tile == 12 or tile == 13:
                aplicar_mecanismo(nuevos_botones, nuevos_puertas, set(), rj, cj)
            estado_hijo = Estado(
                jugador=(rj, cj),
                diamantes=nuevas_diamantes,
                llaves_pos=nuevas_llaves_pos,
                keys=nuevas_llaves,
                puertas=nuevos_puertas,
                pinchos_activados=nuevos_pinchos_activados,
                rocas=nuevas_rocas,
                huecos_rellenos=nuevos_huecos_rellenos,
                botones=nuevos_botones,
                parent=estado,
                accion_from_parent=accion
            )
            sucesores.append((accion, estado_hijo))
            continue
        if tile == 9 and (rn, cn) not in huecos_rellenos:
            continue
        if tile == 10 and (rn, cn) not in huecos_rellenos:
            continue
        nuevas_diamantes = set(diamantes)
        nuevas_llaves_pos = set(llaves_pos)
        nuevas_llaves = keys
        nuevos_puertas = set(puertas)
        nuevos_pinchos_activados = set(pinchos_activados)
        nuevos_rocas = set(rocas)
        nuevos_huecos_rellenos = set(huecos_rellenos)
        nuevos_botones = dict(botones_items)
        if (rn, cn) in nuevas_diamantes:
            nuevas_diamantes.remove((rn, cn))
        if tile == 3 and (rn, cn) in nuevas_llaves_pos:
            if nuevas_llaves == 0:
                nuevas_llaves = 1
                nuevas_llaves_pos.remove((rn, cn))
        if (rn, cn) in puertas and nuevas_llaves > 0:
            nuevos_puertas.remove((rn, cn))
            nuevas_llaves = 0
        if tile == 7 and (rn, cn) not in pinchos_activados:
            nuevos_pinchos_activados.add((rn, cn))
        elif tile == 7:
            continue
        if tile == 8 and nuevas_diamantes:
            continue  # No ir a la salida si quedan diamantes
        if tile == 12 or tile == 13:
            aplicar_mecanismo(nuevos_botones, nuevos_puertas, set(), rn, cn)
        estado_hijo = Estado(
            jugador=(rn, cn),
            diamantes=nuevas_diamantes,
            llaves_pos=nuevas_llaves_pos,
            keys=nuevas_llaves,
            puertas=nuevos_puertas,
            pinchos_activados=nuevos_pinchos_activados,
            rocas=nuevos_rocas,
            huecos_rellenos=nuevos_huecos_rellenos,
            botones=nuevos_botones,
            parent=estado,
            accion_from_parent=accion
        )
        sucesores.append((accion, estado_hijo))
    return sucesores


def heuristico(estado, coord_salida):
    P = estado.jugador
    D = estado.diamantes
    S = coord_salida
    if D:
        dist_min = min(abs(P[0] - dx) + abs(P[1] - dy) for (dx, dy) in D)
        dist_diam_salida = min(abs(dx - S[0]) + abs(dy - S[1]) for (dx, dy) in D)
        return dist_min + dist_diam_salida
    if estado.puertas:
        if estado.keys == 0 and estado.llaves_pos:
            dist_a_llave = min(abs(P[0]-lx)+abs(P[1]-ly) for (lx,ly) in estado.llaves_pos)
            return dist_a_llave + 10
        elif estado.keys == 0 and not estado.llaves_pos:
            return 100
        elif estado.keys > 0:
            puertas = list(estado.puertas)
            dist_a_puerta = min(abs(P[0] - px) + abs(P[1] - py) for (px, py) in puertas)
            dist_puerta_salida = min(abs(px - S[0]) + abs(py - S[1]) for (px, py) in puertas)
            return dist_a_puerta + dist_puerta_salida
    return abs(P[0] - S[0]) + abs(P[1] - S[1])

def a_estrella(estado_inicial, mapa, coord_salida):
    frontera = []
    contador = count()
    g0 = 0
    h0 = heuristico(estado_inicial, coord_salida)
    heapq.heappush(frontera, (g0 + h0, g0, next(contador), estado_inicial))
    mejor_g = {estado_inicial: 0}
    while frontera:
        f_act, g_act, _, estado_act = heapq.heappop(frontera)
        if g_act > mejor_g.get(estado_act, float('inf')):
            continue
        if estado_act.es_meta(coord_salida):
            return estado_act.reconstruir_ruta()
        for accion, estado_sig in vecinos(estado_act, mapa, coord_salida):
            g_sig = g_act + 1
            if g_sig < mejor_g.get(estado_sig, float('inf')):
                mejor_g[estado_sig] = g_sig
                h_sig = heuristico(estado_sig, coord_salida)
                f_sig = g_sig + h_sig
                estado_sig.parent = estado_act
                estado_sig.accion_from_parent = accion
                heapq.heappush(frontera, (f_sig, g_sig, next(contador), estado_sig))
    return None

def main():
    ruta_nivel = "matriz_clasificacion.txt"
    mapa, estado_inicial, coord_salida = parse_level(ruta_nivel)
    print("Resolviendo nivel…")
    solucion = a_estrella(estado_inicial, mapa, coord_salida)
    if solucion is None:
        print("No se encontró solución.")
    else:
        print("Solución encontrada en {} pasos:".format(len(solucion)))
        for i, mov in enumerate(solucion, 1):
            print(f"{i:03d}: {mov}")
        acciones_a_tecla = {
        "arriba": "up",
        "abajo": "down",
        "izquierda": "left",
        "derecha": "right"
        }
        time.sleep(2)
        pyautogui.click(768,656)
        for accion in solucion:
            tecla = acciones_a_tecla.get(accion)
            if tecla:
                pyautogui.keyDown(tecla)
                time.sleep(0.1)
                pyautogui.keyUp(tecla)
                time.sleep(0.1)

if __name__ == "__main__":
    main()
