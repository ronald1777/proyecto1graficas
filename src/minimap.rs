use crate::map::Map;
use crate::player::Player;
use crate::sprite::Enemy;
use raylib::prelude::*;

const CELL_PX: i32 = 6;
const MARGIN: i32 = 14;

/// Dibuja un minimapa en la esquina superior derecha (nunca al lado del
/// mapa principal), mostrando paredes, enemigos vivos y la posicion +
/// direccion del jugador.
pub fn draw_minimap(d: &mut RaylibDrawHandle, map: &Map, player: &Player, enemies: &[Enemy], screen_w: i32) {
    let map_w_px = map.width as i32 * CELL_PX;
    let map_h_px = map.height as i32 * CELL_PX;
    let origin_x = screen_w - map_w_px - MARGIN;
    let origin_y = MARGIN;

    // fondo semi-transparente
    d.draw_rectangle(
        origin_x - 4,
        origin_y - 4,
        map_w_px + 8,
        map_h_px + 8,
        Color::new(0, 0, 0, 160),
    );

    for y in 0..map.height {
        for x in 0..map.width {
            let wall = map.grid[y][x];
            if wall != 0 {
                let color = match wall {
                    1 => Color::new(200, 200, 210, 255),
                    2 => Color::new(120, 200, 140, 255),
                    3 => Color::new(210, 110, 110, 255),
                    _ => Color::GRAY,
                };
                d.draw_rectangle(
                    origin_x + x as i32 * CELL_PX,
                    origin_y + y as i32 * CELL_PX,
                    CELL_PX - 1,
                    CELL_PX - 1,
                    color,
                );
            }
        }
    }

    // meta
    d.draw_rectangle(
        origin_x + map.goal_cell.0 as i32 * CELL_PX,
        origin_y + map.goal_cell.1 as i32 * CELL_PX,
        CELL_PX - 1,
        CELL_PX - 1,
        Color::GOLD,
    );

    // enemigos vivos
    for e in enemies.iter().filter(|e| e.is_alive()) {
        let ex = origin_x + (e.x * CELL_PX as f32) as i32;
        let ey = origin_y + (e.y * CELL_PX as f32) as i32;
        d.draw_circle(ex, ey, 2.5, Color::ORANGE);
    }

    // jugador
    let px = origin_x + (player.x * CELL_PX as f32) as i32;
    let py = origin_y + (player.y * CELL_PX as f32) as i32;
    d.draw_circle(px, py, 3.0, Color::SKYBLUE);
    let (dx, dy) = player.dir();
    d.draw_line(
        px,
        py,
        px + (dx * 10.0) as i32,
        py + (dy * 10.0) as i32,
        Color::SKYBLUE,
    );

    d.draw_rectangle_lines(origin_x - 4, origin_y - 4, map_w_px + 8, map_h_px + 8, Color::WHITE);
}
