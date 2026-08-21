use crate::map::Map;
use raylib::prelude::*;

const MOVE_SPEED: f32 = 3.0; // celdas por segundo
const ROT_SPEED_KEYS: f32 = 2.5; // rad/seg con flechas (respaldo del mouse)
const MOUSE_SENSITIVITY: f32 = 0.0028;
const PLAYER_RADIUS: f32 = 0.2; // radio de colision, en celdas

pub struct Player {
    pub x: f32,
    pub y: f32,
    pub angle: f32, // radianes, 0 = mirando hacia +X
    pub bob_timer: f32,
    pub is_moving: bool,
}

impl Player {
    pub fn new(x: f32, y: f32, angle: f32) -> Self {
        Player {
            x,
            y,
            angle,
            bob_timer: 0.0,
            is_moving: false,
        }
    }

    pub fn dir(&self) -> (f32, f32) {
        (self.angle.cos(), self.angle.sin())
    }

    /// Intenta mover al jugador (dx, dy) en el mundo, resolviendo colisiones
    /// eje por eje para poder "deslizar" contra las paredes sin atravesarlas.
    fn try_move(&mut self, map: &Map, dx: f32, dy: f32) {
        let new_x = self.x + dx;
        if !Self::collides(map, new_x, self.y) {
            self.x = new_x;
        }
        let new_y = self.y + dy;
        if !Self::collides(map, self.x, new_y) {
            self.y = new_y;
        }
    }

    fn collides(map: &Map, x: f32, y: f32) -> bool {
        // revisamos un pequeño cuadrado alrededor del jugador (radio de colision)
        let checks = [
            (x - PLAYER_RADIUS, y - PLAYER_RADIUS),
            (x + PLAYER_RADIUS, y - PLAYER_RADIUS),
            (x - PLAYER_RADIUS, y + PLAYER_RADIUS),
            (x + PLAYER_RADIUS, y + PLAYER_RADIUS),
        ];
        for (cx, cy) in checks {
            if map.is_wall(cx.floor() as i32, cy.floor() as i32) {
                return true;
            }
        }
        false
    }

    pub fn update(&mut self, rl: &RaylibHandle, map: &Map, dt: f32, mouse_locked: bool) {
        let (dirx, diry) = self.dir();
        // perpendicular (derecha del jugador) para strafe
        let (rightx, righty) = (-diry, dirx);

        let mut move_x = 0.0;
        let mut move_y = 0.0;
        let mut moved = false;

        if rl.is_key_down(KeyboardKey::KEY_W) || rl.is_key_down(KeyboardKey::KEY_UP) {
            move_x += dirx;
            move_y += diry;
            moved = true;
        }
        if rl.is_key_down(KeyboardKey::KEY_S) || rl.is_key_down(KeyboardKey::KEY_DOWN) {
            move_x -= dirx;
            move_y -= diry;
            moved = true;
        }
        if rl.is_key_down(KeyboardKey::KEY_D) {
            move_x += rightx;
            move_y += righty;
            moved = true;
        }
        if rl.is_key_down(KeyboardKey::KEY_A) {
            move_x -= rightx;
            move_y -= righty;
            moved = true;
        }

        let len = (move_x * move_x + move_y * move_y).sqrt();
        if len > 0.0001 {
            move_x = move_x / len * MOVE_SPEED * dt;
            move_y = move_y / len * MOVE_SPEED * dt;
            self.try_move(map, move_x, move_y);
        }
        self.is_moving = moved;

        // --- Rotacion horizontal con mouse (objetivo de 20 pts) ---
        if mouse_locked {
            let delta = rl.get_mouse_delta();
            self.angle += delta.x * MOUSE_SENSITIVITY;
        }
        // Respaldo con flechas por si el mouse no esta bloqueado / testing
        if rl.is_key_down(KeyboardKey::KEY_LEFT) {
            self.angle -= ROT_SPEED_KEYS * dt;
        }
        if rl.is_key_down(KeyboardKey::KEY_RIGHT) {
            self.angle += ROT_SPEED_KEYS * dt;
        }

        // bobbing de cabeza al caminar (para el efecto de disparo y ambiente)
        if self.is_moving {
            self.bob_timer += dt * 10.0;
        } else {
            self.bob_timer = 0.0;
        }
    }
}
