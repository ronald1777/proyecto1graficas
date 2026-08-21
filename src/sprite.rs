use crate::map::Map;
use crate::player::Player;
use crate::raycaster::{SCREEN_H, SCREEN_W};
use raylib::prelude::*;

#[derive(PartialEq, Clone, Copy)]
pub enum EnemyState {
    Idle,
    Hit,
    Dead,
}

pub struct Enemy {
    pub x: f32,
    pub y: f32,
    pub state: EnemyState,
    pub anim_timer: f32,
    pub anim_frame: usize, // 0 o 1 para el ciclo idle
    pub hit_timer: f32,
}

impl Enemy {
    pub fn new(x: f32, y: f32) -> Self {
        Enemy {
            x,
            y,
            state: EnemyState::Idle,
            anim_timer: 0.0,
            anim_frame: 0,
            hit_timer: 0.0,
        }
    }

    pub fn update(&mut self, dt: f32) {
        match self.state {
            EnemyState::Idle => {
                // animacion de "respiracion" ciclando entre 2 frames (objetivo de animacion de sprite)
                self.anim_timer += dt;
                if self.anim_timer > 0.45 {
                    self.anim_timer = 0.0;
                    self.anim_frame = 1 - self.anim_frame;
                }
            }
            EnemyState::Hit => {
                self.hit_timer -= dt;
                if self.hit_timer <= 0.0 {
                    self.state = EnemyState::Dead;
                }
            }
            EnemyState::Dead => {}
        }
    }

    pub fn take_hit(&mut self) {
        if self.state == EnemyState::Idle {
            self.state = EnemyState::Hit;
            self.hit_timer = 0.25;
        }
    }

    pub fn is_alive(&self) -> bool {
        self.state != EnemyState::Dead
    }
}

/// Dibuja todos los enemigos vivos como sprites "billboard" (siempre viendo
/// hacia la camara), ocluidos correctamente contra las paredes usando el
/// z-buffer que devolvio el raycaster.
pub fn render_enemies(
    d: &mut RaylibDrawHandle,
    player: &Player,
    enemies: &[Enemy],
    depth_buffer: &[f32],
    idle_textures: &[Texture2D; 2],
    hit_texture: &Texture2D,
) {
    let (dir_x, dir_y) = player.dir();
    let fov_rad = crate::raycaster::FOV_DEG.to_radians();
    let plane_len = (fov_rad / 2.0).tan();
    let plane_x = -dir_y * plane_len;
    let plane_y = dir_x * plane_len;

    // ordenar de mas lejos a mas cerca para pintar correctamente
    let mut order: Vec<usize> = (0..enemies.len()).collect();
    order.sort_by(|&a, &b| {
        let da = dist2(player, &enemies[a]);
        let db = dist2(player, &enemies[b]);
        db.partial_cmp(&da).unwrap()
    });

    for &i in &order {
        let e = &enemies[i];
        if !e.is_alive() {
            continue;
        }
        let rel_x = e.x - player.x;
        let rel_y = e.y - player.y;

        // transformar a espacio de camara
        let inv_det = 1.0 / (plane_x * dir_y - dir_x * plane_y);
        let transform_x = inv_det * (dir_y * rel_x - dir_x * rel_y);
        let transform_y = inv_det * (-plane_y * rel_x + plane_x * rel_y);

        if transform_y <= 0.1 {
            continue; // detras de la camara
        }

        let sprite_screen_x =
            ((SCREEN_W as f32 / 2.0) * (1.0 + transform_x / transform_y)) as i32;

        let sprite_h = (SCREEN_H as f32 / transform_y).abs() as i32;
        let sprite_w = sprite_h; // proporcion aproximada, se ajusta con la textura abajo

        let tex = match e.state {
            EnemyState::Hit => hit_texture,
            _ => &idle_textures[e.anim_frame],
        };
        let aspect = tex.width as f32 / tex.height as f32;
        let draw_w = (sprite_w as f32 * aspect) as i32;

        let draw_start_x = (sprite_screen_x - draw_w / 2).max(0);
        let draw_end_x = (sprite_screen_x + draw_w / 2).min(SCREEN_W - 1);
        let draw_start_y = (SCREEN_H / 2 - sprite_h / 2).max(0);
        let draw_end_y = (SCREEN_H / 2 + sprite_h / 2).min(SCREEN_H - 1);

        if draw_end_x <= draw_start_x {
            continue;
        }

        let shade = (1.0 - (transform_y / 12.0).min(0.7)).max(0.3);
        let s = (shade * 255.0) as u8;
        let tint = Color::new(s, s, s, 255);

        // recortar columna por columna contra el z-buffer para respetar paredes delante
        let stripe_w = ((draw_end_x - draw_start_x) as usize).max(1);
        let step = ((draw_end_x - draw_start_x).max(1)) as f32 / stripe_w as f32;

        let mut sx = draw_start_x;
        while sx < draw_end_x {
            if sx >= 0 && sx < SCREEN_W && transform_y < depth_buffer[sx as usize] {
                let tex_x = (((sx - draw_start_x) as f32 / (draw_end_x - draw_start_x).max(1) as f32)
                    * tex.width as f32) as i32;
                let src = Rectangle::new(tex_x.clamp(0, tex.width - 1) as f32, 0.0, 1.0, tex.height as f32);
                let dest = Rectangle::new(
                    sx as f32,
                    draw_start_y as f32,
                    1.0,
                    (draw_end_y - draw_start_y).max(1) as f32,
                );
                d.draw_texture_pro(tex, src, dest, Vector2::new(0.0, 0.0), 0.0, tint);
            }
            sx += step.max(1.0) as i32;
        }
    }
}

fn dist2(player: &Player, e: &Enemy) -> f32 {
    let dx = e.x - player.x;
    let dy = e.y - player.y;
    dx * dx + dy * dy
}

/// Calcula, con DDA, la distancia hasta la primera pared en la direccion
/// en la que mira el jugador (mismo algoritmo que el raycaster principal
/// pero solo para el rayo central, usado para el chequeo de disparo).
fn wall_distance_ahead(player: &Player, map: &Map) -> f32 {
    let (dir_x, dir_y) = player.dir();
    let mut map_x = player.x.floor() as i32;
    let mut map_y = player.y.floor() as i32;

    let delta_dist_x = if dir_x.abs() < 1e-6 { 1e30 } else { (1.0 / dir_x).abs() };
    let delta_dist_y = if dir_y.abs() < 1e-6 { 1e30 } else { (1.0 / dir_y).abs() };

    let (step_x, mut side_dist_x) = if dir_x < 0.0 {
        (-1, (player.x - map_x as f32) * delta_dist_x)
    } else {
        (1, (map_x as f32 + 1.0 - player.x) * delta_dist_x)
    };
    let (step_y, mut side_dist_y) = if dir_y < 0.0 {
        (-1, (player.y - map_y as f32) * delta_dist_y)
    } else {
        (1, (map_y as f32 + 1.0 - player.y) * delta_dist_y)
    };

    let mut side = 0;
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
        if map.is_wall(map_x, map_y) {
            break;
        }
    }

    if side == 0 {
        (map_x as f32 - player.x + (1 - step_x) as f32 / 2.0) / dir_x
    } else {
        (map_y as f32 - player.y + (1 - step_y) as f32 / 2.0) / dir_y
    }
    .abs()
}

/// Revisa si un disparo (rayo desde el jugador hacia donde mira) impacta
/// a algun enemigo vivo antes de recorrer `max_range` celdas o de chocar
/// con una pared (para que no se pueda disparar "a traves" de un muro).
pub fn try_shoot_simple(player: &Player, enemies: &mut [Enemy], map: &Map, max_range: f32) -> bool {
    let wall_dist = wall_distance_ahead(player, map);

    let mut best: Option<(usize, f32)> = None;
    for (i, e) in enemies.iter().enumerate() {
        if !e.is_alive() {
            continue;
        }
        let dx = e.x - player.x;
        let dy = e.y - player.y;
        let dist = (dx * dx + dy * dy).sqrt();
        if dist > max_range || dist > wall_dist {
            continue; // demasiado lejos o hay una pared en medio
        }
        // angulo entre la mirada del jugador y el enemigo
        let angle_to_enemy = dy.atan2(dx);
        let mut diff = angle_to_enemy - player.angle;
        while diff > std::f32::consts::PI {
            diff -= std::f32::consts::TAU;
        }
        while diff < -std::f32::consts::PI {
            diff += std::f32::consts::TAU;
        }
        // cono de puntería estrecho al centro de la pantalla
        if diff.abs() < 0.09 {
            if best.map_or(true, |(_, bd)| dist < bd) {
                best = Some((i, dist));
            }
        }
    }
    if let Some((idx, _)) = best {
        enemies[idx].take_hit();
        true
    } else {
        false
    }
}
