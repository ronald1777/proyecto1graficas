use raylib::prelude::*;

/// Envuelve el manejo de musica de fondo (en loop) y efectos de sonido.
pub struct AudioManager<'a> {
    pub music: Music<'a>,
    pub sfx_gunshot: Sound<'a>,
    pub sfx_enemy_die: Sound<'a>,
}

impl<'a> AudioManager<'a> {
    pub fn new(rl_audio: &'a RaylibAudio) -> Self {
        let music = rl_audio
            .new_music("assets/music/bg_music.mp3")
            .expect("No se pudo cargar la musica de fondo");
        music.set_volume(0.5);

        let sfx_gunshot = rl_audio
            .new_sound("assets/sfx/gunshot.mp3")
            .expect("No se pudo cargar el sonido de disparo");
        let sfx_enemy_die = rl_audio
            .new_sound("assets/sfx/enemy_die.mp3")
            .expect("No se pudo cargar el sonido de muerte del enemigo");

        AudioManager {
            music,
            sfx_gunshot,
            sfx_enemy_die,
        }
    }

    pub fn start_music(&mut self) {
        self.music.play_stream();
    }

    /// Debe llamarse una vez por frame para que el stream de musica
    /// se mantenga en loop y no se corte.
    pub fn update(&mut self) {
        self.music.update_stream();
        if !self.music.is_stream_playing() {
            self.music.play_stream();
        }
    }

    pub fn play_gunshot(&self) {
        self.sfx_gunshot.play();
    }

    pub fn play_enemy_die(&self) {
        self.sfx_enemy_die.play();
    }
}
