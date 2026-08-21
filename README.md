# ELMATAKARENS — Proyecto de Gráficas por Computadora RONALD CATÚN

Ray caster simple estilo Wolfenstein/DOOM hecho en **Rust** con **raylib-rs**.
Renderiza un nivel completo y jugable, con paredes texturizadas, enemigos
animados, disparo, minimapa, música en loop, efectos de sonido, pantalla de
bienvenida con selección de nivel y pantalla de éxito.

## Requisitos

- Rust (edición 2021 o superior) — `rustc` reciente (1.77+), instalado vía
  `rustup` o el gestor de paquetes de tu distro (en Arch/NyarchLinux:
  `sudo pacman -S rust`).
- Dependencias de sistema para compilar raylib (se compila desde código C
  automáticamente vía `raylib-sys`, no hace falta instalar raylib aparte):
  - Arch: `sudo pacman -S cmake base-devel alsa-lib libx11 libxrandr libxi mesa libxcursor libxinerama pkgconf clang`
  - Ubuntu/Debian: `sudo apt install cmake build-essential libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libxcursor-dev libxinerama-dev pkg-config clang`

## Cómo correrlo

```bash
cargo run --release
```

La primera compilación tarda un poco porque construye raylib desde cero.
Las siguientes son rápidas gracias al caché de `cargo`.

## Controles

| Acción              | Tecla / Input          |
|---------------------|-------------------------|
| Moverse              | `W` `A` `S` `D`          |
| Mirar (rotación)     | Mouse (movimiento horizontal) |
| Disparar              | Click izquierdo          |
| Cambiar nivel (menú)  | Flechas `←` `→`          |
| Empezar / Confirmar   | `ENTER`                  |
| Volver al menú        | `ESC`                    |

## Estructura del proyecto

```
src/
├── main.rs        Loop principal, máquina de estados (menú/juego/éxito), HUD
├── map.rs         Carga de niveles desde texto plano
├── player.rs       Movimiento, colisión, rotación con mouse
├── raycaster.rs     Algoritmo DDA (Lodev) con paredes texturizadas
├── sprite.rs        Enemigos animados (billboards), lógica de disparo
├── minimap.rs        Minimapa en esquina superior derecha
├── audio.rs           Música en loop + efectos de sonido
└── state.rs            Estados del juego y rutas de niveles

levels/level1.txt, level2.txt   Mapas en formato grid ASCII
assets/                          Texturas, sprites, música y sonidos
```

### Formato de los mapas

```
1 2 3   -> tipos de pared distintos (cada uno con su propia textura)
.       -> piso caminable
E       -> posición inicial del jugador
S       -> meta (dispara la pantalla de éxito)
X       -> punto de aparición de un enemigo
```

## Objetivos de la rúbrica cubiertos

| Objetivo | Puntos | Implementación |
|---|---|---|
| Nivel jugable, sin atravesar paredes, sin crashear | requisito | Colisión eje por eje en `player.rs` |
| Color/textura distinta por tipo de pared | requisito | 3 texturas en `assets/textures/`, mapeadas por tipo en `raycaster.rs` |
| Rotación horizontal con mouse | 20 | `player.rs::update` usa `get_mouse_delta()` |
| Disparo | 10 | Click izquierdo, `sprite::try_shoot_simple` |
| Minimapa en esquina (no junto al mapa principal) | 10 | `minimap.rs`, esquina superior derecha |
| Música de fondo en loop | 5 | `audio.rs`, `update_stream()` cada frame + auto-replay |
| Efectos de sonido | 10 | Disparo y muerte de enemigo |
| Animación de sprite | 20 | 2 frames de idle + frame de impacto en `sprite.rs` |
| Pantalla de bienvenida | 5 | `draw_menu` en `main.rs` |
| Selección de múltiples niveles | 10 | Flechas en el menú, 2 niveles |
| Pantalla de éxito | 10 | Al pisar la celda `S` del mapa |
| Estética del nivel | subjetivo (0-30) | Diseño de mapas + shading por distancia |
