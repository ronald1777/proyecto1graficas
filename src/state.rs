/// Maquina de estados simple del juego.
pub enum GameState {
    /// Pantalla de bienvenida con seleccion de nivel (flechas + ENTER)
    Menu { selected_level: usize },
    /// Jugando el nivel actual
    Playing,
    /// Pantalla de exito al llegar a la meta
    Success,
}

pub const LEVEL_PATHS: [&str; 2] = ["levels/level1.txt", "levels/level2.txt"];
pub const LEVEL_NAMES: [&str; 2] = ["Nivel 1: El Resort", "Nivel 2: La Fortaleza"];
