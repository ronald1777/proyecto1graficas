mod audio;
mod map;
mod minimap;
mod player;
mod raycaster;
mod sprite;
mod state;

use audio::AudioManager;
use map::Map;
use player::Player;
use raylib::prelude::*;
use sprite::Enemy;
use state::{GameState, LEVEL_NAMES, LEVEL_PATHS};
use std::collections::HashMap;

const SHOOT_RANGE: f32 = 8.0;
const SHOOT_COOLDOWN: f32 = 0.35;
const RECOIL_TIME: f32 = 0.12;

fn load_level(index: usize) -> (Map, Vec<Enemy>, Player) {
    let map = Map::load_from_file(LEVEL_PATHS[index]);
    let enemies: Vec<Enemy> = map
        .enemy_spawns
        .iter()
        .map(|&(x, y)| Enemy::new(x, y))
        .collect();
    let player = Player::new(map.player_start.0, map.player_start.1, map.player_start_angle);
    (map, enemies, player)
}

fn main() {
    let (mut rl, thread) = raylib::init()
        .size(raycaster::SCREEN_W, raycaster::SCREEN_H)
        .title("EL MATAKARENS")
        .build();

    rl.set_target_fps(60);

    // --- Audio ---
    let rl_audio = RaylibAudio::init_audio_device().expect("No se pudo iniciar el audio");
    let mut audio_mgr = AudioManager::new(&rl_audio);
    audio_mgr.start_music();

    // --- Texturas de pared (una por tipo, objetivo obligatorio del proyecto) ---
    let mut wall_textures: HashMap<i32, Texture2D> = HashMap::new();
    wall_textures.insert(
        1,
        rl.load_texture(&thread, "assets/textures/wall1.png")
            .expect("No se pudo cargar wall1.png"),
    );
    wall_textures.insert(
        2,
        rl.load_texture(&thread, "assets/textures/wall2.png")
            .expect("No se pudo cargar wall2.png"),
    );
    wall_textures.insert(
        3,
        rl.load_texture(&thread, "assets/textures/wall3.png")
            .expect("No se pudo cargar wall3.png"),
    );

    // --- Sprites de enemigo (2 frames idle + 1 frame de impacto = animacion) ---
    let enemy_idle_textures = [
        rl.load_texture(&thread, "assets/sprites/enemy_idle1.png")
            .expect("No se pudo cargar enemy_idle1.png"),
        rl.load_texture(&thread, "assets/sprites/enemy_idle2.png")
            .expect("No se pudo cargar enemy_idle2.png"),
    ];
    let enemy_hit_texture = rl
        .load_texture(&thread, "assets/sprites/enemy_hit.png")
        .expect("No se pudo cargar enemy_hit.png");

    // --- Sprite del arma en primera persona ---
    let hand_gun_texture = rl
        .load_texture(&thread, "assets/sprites/hand_gun.png")
        .expect("No se pudo cargar hand_gun.png");

    // --- Estado inicial ---
    let mut state = GameState::Menu { selected_level: 0 };
    let mut current_level_idx = 0usize;
    let (mut map, mut enemies, mut player) = load_level(current_level_idx);

    let mut shoot_cooldown_timer = 0.0f32;
    let mut recoil_timer = 0.0f32;
    let mut mouse_locked = false;

    while !rl.window_should_close() {
        let dt = rl.get_frame_time();
        audio_mgr.update();

        match &mut state {
            GameState::Menu { selected_level } => {
                if mouse_locked {
                    rl.enable_cursor();
                    mouse_locked = false;
                }
                if rl.is_key_pressed(KeyboardKey::KEY_RIGHT) || rl.is_key_pressed(KeyboardKey::KEY_D) {
                    *selected_level = (*selected_level + 1) % LEVEL_PATHS.len();
                }
                if rl.is_key_pressed(KeyboardKey::KEY_LEFT) || rl.is_key_pressed(KeyboardKey::KEY_A) {
                    *selected_level = (*selected_level + LEVEL_PATHS.len() - 1) % LEVEL_PATHS.len();
                }
                if rl.is_key_pressed(KeyboardKey::KEY_ENTER) {
                    current_level_idx = *selected_level;
                    let (m, e, p) = load_level(current_level_idx);
                    map = m;
                    enemies = e;
                    player = p;
                    state = GameState::Playing;
                }
            }
            GameState::Playing => {
                if !mouse_locked {
                    rl.disable_cursor();
                    mouse_locked = true;
                }
                if rl.is_key_pressed(KeyboardKey::KEY_ESCAPE) {
                    rl.enable_cursor();
                    mouse_locked = false;
                    state = GameState::Menu {
                        selected_level: current_level_idx,
                    };
                }

                player.update(&rl, &map, dt, mouse_locked);

                for e in enemies.iter_mut() {
                    e.update(dt);
                }

                if shoot_cooldown_timer > 0.0 {
                    shoot_cooldown_timer -= dt;
                }
                if recoil_timer > 0.0 {
                    recoil_timer -= dt;
                }

                if rl.is_mouse_button_pressed(MouseButton::MOUSE_BUTTON_LEFT)
                    && shoot_cooldown_timer <= 0.0
                {
                    shoot_cooldown_timer = SHOOT_COOLDOWN;
                    recoil_timer = RECOIL_TIME;
                    audio_mgr.play_gunshot();
                    let hit = sprite::try_shoot_simple(&player, &mut enemies, &map, SHOOT_RANGE);
                    if hit {
                        audio_mgr.play_enemy_die();
                    }
                }

                if map.is_goal(player.x, player.y) {
                    rl.enable_cursor();
                    mouse_locked = false;
                    state = GameState::Success;
                }
            }
            GameState::Success => {
                if mouse_locked {
                    rl.enable_cursor();
                    mouse_locked = false;
                }
                if rl.is_key_pressed(KeyboardKey::KEY_ENTER) {
                    state = GameState::Menu {
                        selected_level: current_level_idx,
                    };
                }
            }
        }

        // ---------------- RENDER ----------------
        let mut d = rl.begin_drawing(&thread);
        d.clear_background(Color::BLACK);

        match &state {
            GameState::Menu { selected_level } => {
                draw_menu(&mut d, *selected_level);
            }
            GameState::Playing => {
                let depth_buffer = raycaster::render_scene(&mut d, &map, &player, &wall_textures);
                sprite::render_enemies(
                    &mut d,
                    &player,
                    &enemies,
                    &depth_buffer,
                    &enemy_idle_textures,
                    &enemy_hit_texture,
                );
                minimap::draw_minimap(&mut d, &map, &player, &enemies, raycaster::SCREEN_W);
                draw_gun(&mut d, &hand_gun_texture, recoil_timer, player.bob_timer);
                draw_hud(&mut d, &enemies, current_level_idx);
            }
            GameState::Success => {
                draw_success(&mut d, current_level_idx);
            }
        }

        d.draw_fps(10, raycaster::SCREEN_H - 24);
    }
}

fn draw_menu(d: &mut RaylibDrawHandle, selected_level: usize) {
    d.clear_background(Color::new(15, 15, 25, 255));
    let w = raycaster::SCREEN_W;

    let title = "EL MATAKARENS";
    let title_size = 64;
    let title_w = d.measure_text(title, title_size);
    d.draw_text(title, w / 2 - title_w / 2, 90, title_size, Color::GOLD);

    let subtitle = "Proyecto de Graficas por Computadora";
    let sub_w = d.measure_text(subtitle, 22);
    d.draw_text(subtitle, w / 2 - sub_w / 2, 165, 22, Color::LIGHTGRAY);

    d.draw_text("Selecciona un nivel:", w / 2 - 140, 260, 26, Color::WHITE);

    let level_name = LEVEL_NAMES[selected_level];
    let lw = d.measure_text(level_name, 34);
    d.draw_text("<", w / 2 - lw / 2 - 50, 300, 34, Color::SKYBLUE);
    d.draw_text(level_name, w / 2 - lw / 2, 300, 34, Color::YELLOW);
    d.draw_text(">", w / 2 + lw / 2 + 30, 300, 34, Color::SKYBLUE);

    let hint = "[FLECHAS] Cambiar nivel   [ENTER] Empezar   [MOUSE] Mirar   [CLICK] Disparar";
    let hint_w = d.measure_text(hint, 18);
    d.draw_text(hint, w / 2 - hint_w / 2, 420, 18, Color::GRAY);

    let hint2 = "WASD para moverse - ESC para volver al menu";
    let hint2_w = d.measure_text(hint2, 18);
    d.draw_text(hint2, w / 2 - hint2_w / 2, 448, 18, Color::GRAY);
}

fn draw_gun(d: &mut RaylibDrawHandle, tex: &Texture2D, recoil_timer: f32, bob_timer: f32) {
    let scale = 1.5;
    let w = (tex.width as f32 * scale) as i32;
    let h = (tex.height as f32 * scale) as i32;
    // El cañon esta desplazado hacia la izquierda dentro del sprite;
    // compensamos su posicion para alinearlo con la mira central.
    let base_x = raycaster::SCREEN_W / 2 - w / 2 + 100;
    let base_y = raycaster::SCREEN_H - h + 40;

    let recoil_offset = if recoil_timer > 0.0 {
        (recoil_timer / RECOIL_TIME) * 30.0
    } else {
        0.0
    };
    let bob_offset = (bob_timer.sin() * 8.0) as i32;

    let dest = Rectangle::new(
        base_x as f32,
        base_y as f32 + recoil_offset + bob_offset as f32,
        w as f32,
        h as f32,
    );
    let src = Rectangle::new(0.0, 0.0, tex.width as f32, tex.height as f32);
    d.draw_texture_pro(tex, src, dest, Vector2::new(0.0, 0.0), 0.0, Color::WHITE);
}

fn draw_hud(d: &mut RaylibDrawHandle, enemies: &[Enemy], level_idx: usize) {
    let alive = enemies.iter().filter(|e| e.is_alive()).count();
    let text = format!(
        "{}  |  Enemigos restantes: {}",
        LEVEL_NAMES[level_idx], alive
    );
    d.draw_text(&text, 10, 10, 20, Color::WHITE);
    let cx = raycaster::SCREEN_W / 2;
    let cy = raycaster::SCREEN_H / 2;
    d.draw_line(cx - 8, cy, cx + 8, cy, Color::new(255, 255, 255, 180));
    d.draw_line(cx, cy - 8, cx, cy + 8, Color::new(255, 255, 255, 180));
}

fn draw_success(d: &mut RaylibDrawHandle, level_idx: usize) {
    d.clear_background(Color::new(10, 30, 15, 255));
    let w = raycaster::SCREEN_W;
    let title = "NIVEL COMPLETADO";
    let ts = 56;
    let tw = d.measure_text(title, ts);
    d.draw_text(title, w / 2 - tw / 2, 220, ts, Color::LIME);

    let sub = format!("Terminaste: {}", LEVEL_NAMES[level_idx]);
    let sw = d.measure_text(&sub, 24);
    d.draw_text(&sub, w / 2 - sw / 2, 300, 24, Color::WHITE);

    let hint = "[ENTER] Volver al menu";
    let hw = d.measure_text(hint, 20);
    d.draw_text(hint, w / 2 - hw / 2, 360, 20, Color::GRAY);
}
