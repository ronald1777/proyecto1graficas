use std::fs;

/// Representa el nivel cargado desde un archivo de texto.
/// La grilla guarda 0 para piso, y 1/2/3 para los distintos tipos de pared.
pub struct Map {
    pub grid: Vec<Vec<i32>>,
    pub width: usize,
    pub height: usize,
    pub player_start: (f32, f32),
    pub player_start_angle: f32,
    pub goal_cell: (usize, usize),
    pub enemy_spawns: Vec<(f32, f32)>,
}

impl Map {
    /// Carga un nivel desde un archivo .txt con el formato:
    /// '1','2','3' = tipos de pared distintos
    /// '.'         = piso caminable
    /// 'E'         = posicion inicial del jugador
    /// 'S'         = meta (pantalla de exito)
    /// 'X'         = punto de spawn de un enemigo
    pub fn load_from_file(path: &str) -> Map {
        let content = fs::read_to_string(path)
            .unwrap_or_else(|_| panic!("No se pudo leer el nivel: {}", path));

        let lines: Vec<&str> = content.lines().filter(|l| !l.is_empty()).collect();
        let height = lines.len();
        let width = lines.iter().map(|l| l.len()).max().unwrap_or(0);

        let mut grid = vec![vec![0i32; width]; height];
        let mut player_start = (1.5, 1.5);
        let mut goal_cell = (0usize, 0usize);
        let mut enemy_spawns = Vec::new();

        for (y, line) in lines.iter().enumerate() {
            for (x, ch) in line.chars().enumerate() {
                match ch {
                    '1' => grid[y][x] = 1,
                    '2' => grid[y][x] = 2,
                    '3' => grid[y][x] = 3,
                    '.' => grid[y][x] = 0,
                    'E' => {
                        grid[y][x] = 0;
                        player_start = (x as f32 + 0.5, y as f32 + 0.5);
                    }
                    'S' => {
                        grid[y][x] = 0;
                        goal_cell = (x, y);
                    }
                    'X' => {
                        grid[y][x] = 0;
                        enemy_spawns.push((x as f32 + 0.5, y as f32 + 0.5));
                    }
                    _ => grid[y][x] = 0,
                }
            }
        }

        Map {
            grid,
            width,
            height,
            player_start,
            player_start_angle: 0.0,
            goal_cell,
            enemy_spawns,
        }
    }

    /// Devuelve el tipo de pared en una celda (0 = no hay pared).
    #[inline]
    pub fn wall_at(&self, x: i32, y: i32) -> i32 {
        if x < 0 || y < 0 || x as usize >= self.width || y as usize >= self.height {
            return 1; // fuera del mapa cuenta como pared solida
        }
        self.grid[y as usize][x as usize]
    }

    #[inline]
    pub fn is_wall(&self, x: i32, y: i32) -> bool {
        self.wall_at(x, y) != 0
    }

    /// Verdadero si (cx, cy) en coordenadas de celda es la celda meta.
    pub fn is_goal(&self, x: f32, y: f32) -> bool {
        let cx = x.floor() as i32;
        let cy = y.floor() as i32;
        cx == self.goal_cell.0 as i32 && cy == self.goal_cell.1 as i32
    }
}
