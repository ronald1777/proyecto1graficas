use crate::map::Map;
use crate::player::Player;
use raylib::prelude::*;
use std::collections::HashMap;

pub const FOV_DEG: f32 = 66.0;

pub const SCREEN_W: i32 = 1024;
pub const SCREEN_H: i32 = 640;

const CEIL_COLOR: Color = Color::new(35, 35, 45, 255);
const FLOOR_COLOR: Color = Color::new(55, 45, 40, 255);

/// Lanza un rayo por columna de pantalla usando DDA (algoritmo de Lodev),
/// dibuja paredes texturizadas y devuelve el z-buffer (distancia por columna)
/// para poder ocluir correctamente los sprites despues.
pub fn render_scene(
    d: &mut RaylibDrawHandle,
    map: &Map,
    player: &Player,
    textures: &HashMap<i32, Texture2D>,
) -> Vec<f32> {
    let w = SCREEN_W;
    let h = SCREEN_H;

    // techo y piso
    d.draw_rectangle(0, 0, w, h / 2, CEIL_COLOR);
    d.draw_rectangle(0, h / 2, w, h / 2, FLOOR_COLOR);

    let mut depth_buffer = vec![f32::MAX; w as usize];

    let (dir_x, dir_y) = player.dir();
    let fov_rad = FOV_DEG.to_radians();
    let plane_len = (fov_rad / 2.0).tan();
    // plano de camara, perpendicular a la direccion
    let plane_x = -dir_y * plane_len;
    let plane_y = dir_x * plane_len;

    for x in 0..w {
        let camera_x = 2.0 * x as f32 / w as f32 - 1.0;
        let ray_dir_x = dir_x + plane_x * camera_x;
        let ray_dir_y = dir_y + plane_y * camera_x;

        let mut map_x = player.x.floor() as i32;
        let mut map_y = player.y.floor() as i32;

        let delta_dist_x = if ray_dir_x.abs() < 1e-6 { 1e30 } else { (1.0 / ray_dir_x).abs() };
        let delta_dist_y = if ray_dir_y.abs() < 1e-6 { 1e30 } else { (1.0 / ray_dir_y).abs() };

        let (step_x, mut side_dist_x) = if ray_dir_x < 0.0 {
            (-1, (player.x - map_x as f32) * delta_dist_x)
        } else {
            (1, (map_x as f32 + 1.0 - player.x) * delta_dist_x)
        };
        let (step_y, mut side_dist_y) = if ray_dir_y < 0.0 {
            (-1, (player.y - map_y as f32) * delta_dist_y)
        } else {
            (1, (map_y as f32 + 1.0 - player.y) * delta_dist_y)
        };

        let mut side; // 0 = golpe en pared vertical (eje X), 1 = horizontal (eje Y)
        let mut wall_type;
        loop {
            if side_dist_x < side_dist_y {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
            } else {
                side_dist_y += delta_dist_y;
                map_y += step_y;
                side = 1;
            }
            wall_type = map.wall_at(map_x, map_y);
            if wall_type != 0 {
                break;
            }
        }

        let perp_dist = if side == 0 {
            (map_x as f32 - player.x + (1 - step_x) as f32 / 2.0) / ray_dir_x
        } else {
            (map_y as f32 - player.y + (1 - step_y) as f32 / 2.0) / ray_dir_y
        };
        let perp_dist = perp_dist.max(0.0001);
        depth_buffer[x as usize] = perp_dist;

        let line_height = (h as f32 / perp_dist) as i32;
        let draw_start = (-line_height / 2 + h / 2).max(0);
        let draw_end = (line_height / 2 + h / 2).min(h - 1);

        // coordenada X dentro de la textura donde golpeo el rayo
        let mut wall_x = if side == 0 {
            player.y + perp_dist * ray_dir_y
        } else {
            player.x + perp_dist * ray_dir_x
        };
        wall_x -= wall_x.floor();

        if let Some(tex) = textures.get(&wall_type) {
            let tex_w = tex.width as f32;
            let tex_h = tex.height as f32;
            let mut tex_x = (wall_x * tex_w) as i32;
            if (side == 0 && ray_dir_x > 0.0) || (side == 1 && ray_dir_y < 0.0) {
                tex_x = tex_w as i32 - tex_x - 1;
            }
            tex_x = tex_x.clamp(0, tex_w as i32 - 1);

            // sombreado por distancia + lados Y mas oscuros (estilo Wolfenstein)
            let shade = (1.0 - (perp_dist / 12.0).min(0.75)).max(0.25);
            let side_factor = if side == 1 { 0.7 } else { 1.0 };
            let s = (shade * side_factor * 255.0) as u8;
            let tint = Color::new(s, s, s, 255);

            let src = Rectangle::new(tex_x as f32, 0.0, 1.0, tex_h);
            let dest = Rectangle::new(
                x as f32,
                draw_start as f32,
                1.0,
                (draw_end - draw_start).max(1) as f32,
            );
            d.draw_texture_pro(tex, src, dest, Vector2::new(0.0, 0.0), 0.0, tint);
        }
    }

    depth_buffer
}
