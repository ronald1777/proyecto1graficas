#include <pspkernel.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <pspaudiolib.h>
#include <pspmp3.h>
#include <psputility.h>
#include <raylib.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("EL MATAKARENS", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define SCREEN_W 480
#define SCREEN_H 272
#define MAX_MAP_W 20
#define MAX_MAP_H 20
#define MAX_ENEMIES 16
#define FOV_DEG 66.0f
#define PI_F 3.14159265358979323846f
#define TAU_F (2.0f * PI_F)
#define MOVE_SPEED 2.7f
#define ROT_SPEED 2.4f
#define PLAYER_RADIUS 0.20f
#define SHOOT_RANGE 8.0f
#define SHOOT_COOLDOWN 0.35f
#define RECOIL_TIME 0.12f
#define RENDER_W 160
#define RENDER_H 91

#define EMBEDDED_ASSET(name) \
    extern unsigned char _binary_assets_##name##_start[]; \
    extern unsigned char _binary_assets_##name##_end[]

EMBEDDED_ASSET(wall1_png);
EMBEDDED_ASSET(wall2_png);
EMBEDDED_ASSET(wall3_png);
EMBEDDED_ASSET(enemy_idle1_png);
EMBEDDED_ASSET(enemy_idle2_png);
EMBEDDED_ASSET(enemy_hit_png);
EMBEDDED_ASSET(hand_gun_png);
EMBEDDED_ASSET(bg_music_mp3);
EMBEDDED_ASSET(gunshot_pcm);
EMBEDDED_ASSET(enemy_die_pcm);

#define ASSET_START(name) _binary_assets_##name##_start
#define ASSET_SIZE(name) ((int)(_binary_assets_##name##_end - _binary_assets_##name##_start))

typedef enum { STATE_MENU, STATE_PLAYING, STATE_SUCCESS } GameState;
typedef enum { ENEMY_IDLE, ENEMY_HIT, ENEMY_DEAD } EnemyState;

typedef struct {
    int grid[MAX_MAP_H][MAX_MAP_W];
    int width;
    int height;
    float start_x;
    float start_y;
    int goal_x;
    int goal_y;
    float enemy_x[MAX_ENEMIES];
    float enemy_y[MAX_ENEMIES];
    int enemy_count;
} Map;

typedef struct {
    float x;
    float y;
    float angle;
    float bob_timer;
    bool moving;
} Player;

typedef struct {
    float x;
    float y;
    EnemyState state;
    float anim_timer;
    int anim_frame;
    float hit_timer;
} Enemy;

static const char *LEVEL_NAMES[2] = {
    "Nivel 1: El Resort",
    "Nivel 2: La Fortaleza"
};

static const char *LEVEL_1[] = {
    "111111111111111",
    "1E............1",
    "1...22222.....1",
    "1.X.1.........1",
    "1...1....3333.1",
    "1..........3..1",
    "1......1...3..1",
    "1......1..X...1",
    "1.22...1......1",
    "1............S1",
    "111111111111111"
};

static const char *LEVEL_2[] = {
    "11111111111111111",
    "1E..............1",
    "1.2........1....1",
    "1.2.33333..1....1",
    "1...3......1....1",
    "1...3X..3.......1",
    "1...33333..1....1",
    "1..........1....1",
    "1..........2222.1",
    "1.11.11....1....1",
    "1.............X.1",
    "1..............S1",
    "11111111111111111"
};

typedef struct {
    Color *pixels;
    int width;
    int height;
} CpuImage;

static CpuImage wall_images[3];
static CpuImage enemy_idle_images[2];
static CpuImage enemy_hit_image;
static Texture2D hand_gun_texture;
static Texture2D scene_texture;
static Color scene_pixels[RENDER_W * RENDER_H] __attribute__((aligned(64)));
static float depth_buffer[RENDER_W];

#define MP3_STREAM_BUFFER_SIZE (16 * 1024)
#define MP3_PCM_BUFFER_SIZE (16 * (1152 / 2))
#define AUDIO_VOLUME 0x5000

static unsigned char mp3_stream_buffer[MP3_STREAM_BUFFER_SIZE] __attribute__((aligned(64)));
static unsigned char mp3_pcm_buffer[MP3_PCM_BUFFER_SIZE] __attribute__((aligned(64)));
static volatile bool audio_running = false;
static int music_thread_id = -1;
static volatile int gunshot_frame = -1;
static volatile int enemy_die_frame = -1;

static float clampf(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int clampi(int value, int minimum, int maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int raylib_button(unsigned int psp_button) {
    switch (psp_button) {
        case PSP_CTRL_CROSS: return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        case PSP_CTRL_CIRCLE: return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
        case PSP_CTRL_SQUARE: return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
        case PSP_CTRL_TRIANGLE: return GAMEPAD_BUTTON_RIGHT_FACE_UP;
        case PSP_CTRL_UP: return GAMEPAD_BUTTON_LEFT_FACE_UP;
        case PSP_CTRL_RIGHT: return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
        case PSP_CTRL_DOWN: return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
        case PSP_CTRL_LEFT: return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
        case PSP_CTRL_LTRIGGER: return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
        case PSP_CTRL_RTRIGGER: return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
        case PSP_CTRL_SELECT: return GAMEPAD_BUTTON_MIDDLE_LEFT;
        case PSP_CTRL_START: return GAMEPAD_BUTTON_MIDDLE_RIGHT;
        default: return GAMEPAD_BUTTON_UNKNOWN;
    }
}

static bool button_pressed(unsigned int button) {
    return IsGamepadButtonPressed(0, raylib_button(button));
}

static bool button_down(unsigned int button) {
    return IsGamepadButtonDown(0, raylib_button(button));
}

static Texture2D load_embedded_png(const unsigned char *data, int size) {
    Image image = LoadImageFromMemory(".png", data, size);
    if (image.data == NULL) {
        // Si el PNG no se pudo decodificar, usamos un cuadro magenta bien
        // visible en vez de dejar una textura invalida que puede crashear
        // al dibujarse (esto evita apagones "silenciosos" en PSP real).
        image = GenImageColor(2, 2, MAGENTA);
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

static CpuImage load_cpu_png(const unsigned char *data, int size) {
    CpuImage result = {0};
    Image image = LoadImageFromMemory(".png", data, size);
    if (image.data == NULL) image = GenImageColor(2, 2, MAGENTA);
    result.width = image.width;
    result.height = image.height;
    result.pixels = LoadImageColors(image);
    UnloadImage(image);
    return result;
}

static Color sample_image(const CpuImage *image, int x, int y) {
    x = clampi(x, 0, image->width - 1);
    y = clampi(y, 0, image->height - 1);
    return image->pixels[y * image->width + x];
}

static Color tint_color(Color color, unsigned char light) {
    color.r = (unsigned char)(((unsigned int)color.r * light) / 255);
    color.g = (unsigned char)(((unsigned int)color.g * light) / 255);
    color.b = (unsigned char)(((unsigned int)color.b * light) / 255);
    return color;
}

static void blend_scene_pixel(int x, int y, Color source, unsigned char light) {
    if (x < 0 || x >= RENDER_W || y < 0 || y >= RENDER_H || source.a == 0) return;
    source = tint_color(source, light);
    Color *destination = &scene_pixels[y * RENDER_W + x];
    if (source.a == 255) {
        *destination = source;
        return;
    }
    unsigned int alpha = source.a;
    unsigned int inverse = 255 - alpha;
    destination->r = (unsigned char)((source.r * alpha + destination->r * inverse) / 255);
    destination->g = (unsigned char)((source.g * alpha + destination->g * inverse) / 255);
    destination->b = (unsigned char)((source.b * alpha + destination->b * inverse) / 255);
    destination->a = 255;
}

static short clamp_sample(int sample) {
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return (short)sample;
}

static void sound_effect_callback(void *buffer, unsigned int frames, void *user_data) {
    (void)user_data;
    short *output = (short *)buffer;
    const short *gunshot = (const short *)ASSET_START(gunshot_pcm);
    const short *enemy_die = (const short *)ASSET_START(enemy_die_pcm);
    int gunshot_frames = ASSET_SIZE(gunshot_pcm) / (int)(sizeof(short) * 2);
    int enemy_die_frames = ASSET_SIZE(enemy_die_pcm) / (int)(sizeof(short) * 2);

    for (unsigned int i = 0; i < frames; i++) {
        int left = 0;
        int right = 0;
        int position = gunshot_frame;
        if (position >= 0 && position < gunshot_frames) {
            left += gunshot[position * 2];
            right += gunshot[position * 2 + 1];
            gunshot_frame = position + 1;
        } else if (position >= gunshot_frames) gunshot_frame = -1;

        position = enemy_die_frame;
        if (position >= 0 && position < enemy_die_frames) {
            left += enemy_die[position * 2];
            right += enemy_die[position * 2 + 1];
            enemy_die_frame = position + 1;
        } else if (position >= enemy_die_frames) enemy_die_frame = -1;

        output[i * 2] = clamp_sample(left);
        output[i * 2 + 1] = clamp_sample(right);
    }
}

static int fill_mp3_stream(int handle) {
    unsigned char *destination = NULL;
    SceInt32 bytes_requested = 0;
    SceInt32 source_position = 0;
    int status = sceMp3GetInfoToAddStreamData(handle, &destination,
                                               &bytes_requested, &source_position);
    if (status < 0 || bytes_requested <= 0) return 0;
    int music_size = ASSET_SIZE(bg_music_mp3);
    if (source_position < 0 || source_position >= music_size) return 0;
    int remaining = music_size - source_position;
    int bytes_to_copy = bytes_requested < remaining ? bytes_requested : remaining;
    memcpy(destination, ASSET_START(bg_music_mp3) + source_position, bytes_to_copy);
    if (sceMp3NotifyAddStreamData(handle, bytes_to_copy) < 0) return 0;
    return bytes_to_copy;
}

static int music_thread(SceSize argument_size, void *arguments) {
    (void)argument_size;
    (void)arguments;
    int handle = -1;
    int channel = -1;

    if (sceMp3InitResource() < 0) return 0;
    SceMp3InitArg init = {
        .mp3StreamStart = 0,
        .mp3StreamEnd = ASSET_SIZE(bg_music_mp3),
        .mp3Buf = mp3_stream_buffer,
        .mp3BufSize = sizeof(mp3_stream_buffer),
        .pcmBuf = mp3_pcm_buffer,
        .pcmBufSize = sizeof(mp3_pcm_buffer)
    };
    handle = sceMp3ReserveMp3Handle(&init);
    if (handle < 0) {
        sceMp3TermResource();
        return 0;
    }
    fill_mp3_stream(handle);
    if (sceMp3Init(handle) < 0) {
        sceMp3ReleaseMp3Handle(handle);
        sceMp3TermResource();
        return 0;
    }

    sceMp3SetLoopNum(handle, -1);
    int sample_rate = sceMp3GetSamplingRate(handle);
    int channels = sceMp3GetMp3ChannelNum(handle);
    int last_decoded = 0;

    while (audio_running) {
        if (sceMp3CheckStreamDataNeeded(handle) > 0) fill_mp3_stream(handle);
        short *decoded_buffer = NULL;
        int bytes_decoded = sceMp3Decode(handle, &decoded_buffer);
        if (bytes_decoded > 0) {
            if (channel < 0 || bytes_decoded != last_decoded) {
                if (channel >= 0) sceAudioSRCChRelease();
                channel = sceAudioSRCChReserve(bytes_decoded / (2 * channels),
                                               sample_rate, channels);
                last_decoded = bytes_decoded;
            }
            if (channel >= 0) sceAudioSRCOutputBlocking(AUDIO_VOLUME, decoded_buffer);
        } else {
            sceMp3ResetPlayPosition(handle);
            fill_mp3_stream(handle);
            sceKernelDelayThread(1000);
        }
    }

    if (channel >= 0) sceAudioSRCChRelease();
    sceMp3ReleaseMp3Handle(handle);
    sceMp3TermResource();
    return 0;
}

static void init_audio(void) {
    int codec_status = sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
    int mp3_status = sceUtilityLoadModule(PSP_MODULE_AV_MP3);
    if (pspAudioInit() >= 0) {
        pspAudioSetVolume(0, PSP_VOLUME_MAX, PSP_VOLUME_MAX);
        pspAudioSetChannelCallback(0, sound_effect_callback, NULL);
    }
    if (codec_status >= 0 && mp3_status >= 0) {
        audio_running = true;
        music_thread_id = sceKernelCreateThread("music_thread", music_thread,
                                                0x12, 0x10000, 0, NULL);
        if (music_thread_id >= 0) sceKernelStartThread(music_thread_id, 0, NULL);
        else audio_running = false;
    }
}

static void stop_audio(void) {
    audio_running = false;
    if (music_thread_id >= 0) sceKernelWaitThreadEnd(music_thread_id, NULL);
    pspAudioEndPre();
    pspAudioEnd();
}

static void play_gunshot(void) {
    gunshot_frame = 0;
}

static void play_enemy_die(void) {
    enemy_die_frame = 0;
}

static void load_map(Map *map, int level) {
    const char **rows = level == 0 ? LEVEL_1 : LEVEL_2;
    int height = level == 0 ? (int)(sizeof(LEVEL_1) / sizeof(LEVEL_1[0]))
                            : (int)(sizeof(LEVEL_2) / sizeof(LEVEL_2[0]));
    memset(map, 0, sizeof(*map));
    map->height = height;
    map->width = (int)strlen(rows[0]);
    map->start_x = 1.5f;
    map->start_y = 1.5f;

    for (int y = 0; y < height; y++) {
        int row_width = (int)strlen(rows[y]);
        if (row_width > map->width) map->width = row_width;
        for (int x = 0; x < row_width && x < MAX_MAP_W; x++) {
            char tile = rows[y][x];
            if (tile >= '1' && tile <= '3') map->grid[y][x] = tile - '0';
            else if (tile == 'E') {
                map->start_x = x + 0.5f;
                map->start_y = y + 0.5f;
            } else if (tile == 'S') {
                map->goal_x = x;
                map->goal_y = y;
            } else if (tile == 'X' && map->enemy_count < MAX_ENEMIES) {
                int i = map->enemy_count++;
                map->enemy_x[i] = x + 0.5f;
                map->enemy_y[i] = y + 0.5f;
            }
        }
    }
}

static int wall_at(const Map *map, int x, int y) {
    if (x < 0 || y < 0 || x >= map->width || y >= map->height) return 1;
    return map->grid[y][x];
}

static bool collides(const Map *map, float x, float y) {
    const float offsets[4][2] = {
        {-PLAYER_RADIUS, -PLAYER_RADIUS}, {PLAYER_RADIUS, -PLAYER_RADIUS},
        {-PLAYER_RADIUS, PLAYER_RADIUS}, {PLAYER_RADIUS, PLAYER_RADIUS}
    };
    for (int i = 0; i < 4; i++) {
        int cx = (int)floorf(x + offsets[i][0]);
        int cy = (int)floorf(y + offsets[i][1]);
        if (wall_at(map, cx, cy) != 0) return true;
    }
    return false;
}

static void try_move(Player *player, const Map *map, float dx, float dy) {
    float new_x = player->x + dx;
    if (!collides(map, new_x, player->y)) player->x = new_x;
    float new_y = player->y + dy;
    if (!collides(map, player->x, new_y)) player->y = new_y;
}

static void reset_level(Map *map, Player *player, Enemy enemies[MAX_ENEMIES], int level) {
    load_map(map, level);
    player->x = map->start_x;
    player->y = map->start_y;
    player->angle = 0.0f;
    player->bob_timer = 0.0f;
    player->moving = false;
    memset(enemies, 0, sizeof(Enemy) * MAX_ENEMIES);
    for (int i = 0; i < map->enemy_count; i++) {
        enemies[i].x = map->enemy_x[i];
        enemies[i].y = map->enemy_y[i];
        enemies[i].state = ENEMY_IDLE;
    }
}

static void update_player(Player *player, const Map *map, float dt) {
    float dir_x = cosf(player->angle);
    float dir_y = sinf(player->angle);
    float right_x = -dir_y;
    float right_y = dir_x;
    float move_x = 0.0f;
    float move_y = 0.0f;

    float analog_x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
    float analog_y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
    if (fabsf(analog_x) < 0.18f) analog_x = 0.0f;
    if (fabsf(analog_y) < 0.18f) analog_y = 0.0f;

    float forward = -analog_y;
    if (button_down(PSP_CTRL_UP)) forward += 1.0f;
    if (button_down(PSP_CTRL_DOWN)) forward -= 1.0f;
    float strafe = 0.0f;
    if (button_down(PSP_CTRL_LTRIGGER)) strafe -= 1.0f;
    if (button_down(PSP_CTRL_RTRIGGER)) strafe += 1.0f;

    player->angle += analog_x * ROT_SPEED * dt;
    if (button_down(PSP_CTRL_LEFT)) player->angle -= ROT_SPEED * dt;
    if (button_down(PSP_CTRL_RIGHT)) player->angle += ROT_SPEED * dt;

    move_x = dir_x * forward + right_x * strafe;
    move_y = dir_y * forward + right_y * strafe;
    float length = sqrtf(move_x * move_x + move_y * move_y);
    player->moving = length > 0.01f;
    if (length > 0.01f) {
        try_move(player, map, move_x / length * MOVE_SPEED * dt,
                 move_y / length * MOVE_SPEED * dt);
        player->bob_timer += dt * 10.0f;
    } else {
        player->bob_timer = 0.0f;
    }
}

static void update_enemies(Enemy enemies[MAX_ENEMIES], int count, float dt) {
    for (int i = 0; i < count; i++) {
        Enemy *enemy = &enemies[i];
        if (enemy->state == ENEMY_IDLE) {
            enemy->anim_timer += dt;
            if (enemy->anim_timer > 0.45f) {
                enemy->anim_timer = 0.0f;
                enemy->anim_frame = 1 - enemy->anim_frame;
            }
        } else if (enemy->state == ENEMY_HIT) {
            enemy->hit_timer -= dt;
            if (enemy->hit_timer <= 0.0f) enemy->state = ENEMY_DEAD;
        }
    }
}

static void render_scene(const Map *map, const Player *player) {
    const Color ceiling = {35, 35, 45, 255};
    const Color floor_color = {55, 45, 40, 255};
    for (int y = 0; y < RENDER_H; y++) {
        Color color = y < RENDER_H / 2 ? ceiling : floor_color;
        for (int x = 0; x < RENDER_W; x++) scene_pixels[y * RENDER_W + x] = color;
    }

    float dir_x = cosf(player->angle);
    float dir_y = sinf(player->angle);
    float plane_len = tanf((FOV_DEG * PI_F / 180.0f) / 2.0f);
    float plane_x = -dir_y * plane_len;
    float plane_y = dir_x * plane_len;

    for (int x = 0; x < RENDER_W; x++) {
        float camera_x = 2.0f * x / RENDER_W - 1.0f;
        float ray_x = dir_x + plane_x * camera_x;
        float ray_y = dir_y + plane_y * camera_x;
        int map_x = (int)floorf(player->x);
        int map_y = (int)floorf(player->y);
        float delta_x = fabsf(ray_x) < 0.000001f ? 1.0e30f : fabsf(1.0f / ray_x);
        float delta_y = fabsf(ray_y) < 0.000001f ? 1.0e30f : fabsf(1.0f / ray_y);
        int step_x;
        int step_y;
        float side_x;
        float side_y;
        if (ray_x < 0.0f) { step_x = -1; side_x = (player->x - map_x) * delta_x; }
        else { step_x = 1; side_x = (map_x + 1.0f - player->x) * delta_x; }
        if (ray_y < 0.0f) { step_y = -1; side_y = (player->y - map_y) * delta_y; }
        else { step_y = 1; side_y = (map_y + 1.0f - player->y) * delta_y; }

        int side = 0;
        int wall_type = 0;
        while (wall_type == 0) {
            if (side_x < side_y) { side_x += delta_x; map_x += step_x; side = 0; }
            else { side_y += delta_y; map_y += step_y; side = 1; }
            wall_type = wall_at(map, map_x, map_y);
        }

        float distance = side == 0
            ? (map_x - player->x + (1 - step_x) / 2.0f) / ray_x
            : (map_y - player->y + (1 - step_y) / 2.0f) / ray_y;
        if (distance < 0.0001f) distance = 0.0001f;
        depth_buffer[x] = distance;

        int line_height = (int)(RENDER_H / distance);
        int draw_start = -line_height / 2 + RENDER_H / 2;
        int draw_end = line_height / 2 + RENDER_H / 2;
        if (draw_start < 0) draw_start = 0;
        if (draw_end >= RENDER_H) draw_end = RENDER_H - 1;

        float wall_x = side == 0 ? player->y + distance * ray_y
                                 : player->x + distance * ray_x;
        wall_x -= floorf(wall_x);
        const CpuImage *texture = &wall_images[clampi(wall_type - 1, 0, 2)];
        int tex_x = (int)(wall_x * texture->width);
        if ((side == 0 && ray_x > 0.0f) || (side == 1 && ray_y < 0.0f))
            tex_x = texture->width - tex_x - 1;
        tex_x = clampi(tex_x, 0, texture->width - 1);

        float shade = 1.0f - fminf(distance / 12.0f, 0.75f);
        if (side == 1) shade *= 0.70f;
        unsigned char light = (unsigned char)(clampf(shade, 0.25f, 1.0f) * 255.0f);
        int visible_height = draw_end - draw_start + 1;
        for (int y = draw_start; y <= draw_end; y++) {
            int tex_y = ((y - draw_start) * texture->height) / visible_height;
            scene_pixels[y * RENDER_W + x] = tint_color(sample_image(texture, tex_x, tex_y), light);
        }
    }
}

static float enemy_dist2(const Player *player, const Enemy *enemy) {
    float dx = enemy->x - player->x;
    float dy = enemy->y - player->y;
    return dx * dx + dy * dy;
}

static void render_enemies(const Player *player, Enemy enemies[MAX_ENEMIES], int count) {
    int order[MAX_ENEMIES];
    for (int i = 0; i < count; i++) order[i] = i;
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (enemy_dist2(player, &enemies[order[i]]) < enemy_dist2(player, &enemies[order[j]])) {
                int temp = order[i]; order[i] = order[j]; order[j] = temp;
            }
        }
    }

    float dir_x = cosf(player->angle);
    float dir_y = sinf(player->angle);
    float plane_len = tanf((FOV_DEG * PI_F / 180.0f) / 2.0f);
    float plane_x = -dir_y * plane_len;
    float plane_y = dir_x * plane_len;

    for (int n = 0; n < count; n++) {
        Enemy *enemy = &enemies[order[n]];
        if (enemy->state == ENEMY_DEAD) continue;
        float rel_x = enemy->x - player->x;
        float rel_y = enemy->y - player->y;
        float inv_det = 1.0f / (plane_x * dir_y - dir_x * plane_y);
        float transform_x = inv_det * (dir_y * rel_x - dir_x * rel_y);
        float transform_y = inv_det * (-plane_y * rel_x + plane_x * rel_y);
        if (transform_y <= 0.1f) continue;

        int screen_x = (int)((RENDER_W / 2.0f) * (1.0f + transform_x / transform_y));
        int sprite_h = abs((int)(RENDER_H / transform_y));
        const CpuImage *texture = enemy->state == ENEMY_HIT ? &enemy_hit_image
                                  : &enemy_idle_images[enemy->anim_frame];
        int sprite_w = (int)(sprite_h * ((float)texture->width / texture->height));
        int raw_start_x = screen_x - sprite_w / 2;
        int raw_end_x = screen_x + sprite_w / 2;
        int raw_start_y = RENDER_H / 2 - sprite_h / 2;
        int raw_end_y = RENDER_H / 2 + sprite_h / 2;
        int start_x = clampi(raw_start_x, 0, RENDER_W - 1);
        int end_x = clampi(raw_end_x, 0, RENDER_W - 1);
        int start_y = clampi(raw_start_y, 0, RENDER_H - 1);
        int end_y = clampi(raw_end_y, 0, RENDER_H - 1);
        int full_width = raw_end_x - raw_start_x;
        int full_height = raw_end_y - raw_start_y;
        if (end_x <= start_x || full_width <= 0 || full_height <= 0) continue;

        float shade = clampf(1.0f - fminf(transform_y / 12.0f, 0.70f), 0.30f, 1.0f);
        unsigned char light = (unsigned char)(shade * 255.0f);
        for (int sx = start_x; sx <= end_x; sx++) {
            if (transform_y < depth_buffer[sx]) {
                int tex_x = ((sx - raw_start_x) * texture->width) / full_width;
                for (int sy = start_y; sy <= end_y; sy++) {
                    int tex_y = ((sy - raw_start_y) * texture->height) / full_height;
                    blend_scene_pixel(sx, sy, sample_image(texture, tex_x, tex_y), light);
                }
            }
        }
    }
}

static float wall_distance_ahead(const Player *player, const Map *map) {
    float dir_x = cosf(player->angle);
    float dir_y = sinf(player->angle);
    int map_x = (int)floorf(player->x);
    int map_y = (int)floorf(player->y);
    float delta_x = fabsf(dir_x) < 0.000001f ? 1.0e30f : fabsf(1.0f / dir_x);
    float delta_y = fabsf(dir_y) < 0.000001f ? 1.0e30f : fabsf(1.0f / dir_y);
    int step_x = dir_x < 0.0f ? -1 : 1;
    int step_y = dir_y < 0.0f ? -1 : 1;
    float side_x = dir_x < 0.0f ? (player->x - map_x) * delta_x
                                : (map_x + 1.0f - player->x) * delta_x;
    float side_y = dir_y < 0.0f ? (player->y - map_y) * delta_y
                                : (map_y + 1.0f - player->y) * delta_y;
    int side = 0;
    do {
        if (side_x < side_y) { side_x += delta_x; map_x += step_x; side = 0; }
        else { side_y += delta_y; map_y += step_y; side = 1; }
    } while (wall_at(map, map_x, map_y) == 0);
    float result = side == 0
        ? (map_x - player->x + (1 - step_x) / 2.0f) / dir_x
        : (map_y - player->y + (1 - step_y) / 2.0f) / dir_y;
    return fabsf(result);
}

static bool try_shoot(Player *player, Enemy enemies[MAX_ENEMIES], int count, const Map *map) {
    float wall_distance = wall_distance_ahead(player, map);
    int best = -1;
    float best_distance = 1.0e30f;
    for (int i = 0; i < count; i++) {
        Enemy *enemy = &enemies[i];
        if (enemy->state == ENEMY_DEAD) continue;
        float dx = enemy->x - player->x;
        float dy = enemy->y - player->y;
        float distance = sqrtf(dx * dx + dy * dy);
        if (distance > SHOOT_RANGE || distance > wall_distance) continue;
        float difference = atan2f(dy, dx) - player->angle;
        while (difference > PI_F) difference -= TAU_F;
        while (difference < -PI_F) difference += TAU_F;
        if (fabsf(difference) < 0.09f && distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    if (best >= 0) {
        enemies[best].state = ENEMY_HIT;
        enemies[best].hit_timer = 0.25f;
        return true;
    }
    return false;
}

static void draw_minimap(const Map *map, const Player *player,
                         Enemy enemies[MAX_ENEMIES]) {
    const int cell = 3;
    int width = map->width * cell;
    int height = map->height * cell;
    int origin_x = SCREEN_W - width - 6;
    int origin_y = 6;
    DrawRectangle(origin_x - 2, origin_y - 2, width + 4, height + 4,
                  (Color){0, 0, 0, 180});
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int wall = map->grid[y][x];
            if (wall) {
                Color color = wall == 1 ? (Color){200, 200, 210, 255}
                            : wall == 2 ? (Color){120, 200, 140, 255}
                                        : (Color){210, 110, 110, 255};
                DrawRectangle(origin_x + x * cell, origin_y + y * cell, cell - 1, cell - 1, color);
            }
        }
    }
    DrawRectangle(origin_x + map->goal_x * cell, origin_y + map->goal_y * cell,
                  cell - 1, cell - 1, GOLD);
    for (int i = 0; i < map->enemy_count; i++) {
        if (enemies[i].state != ENEMY_DEAD)
            DrawCircle(origin_x + (int)(enemies[i].x * cell),
                       origin_y + (int)(enemies[i].y * cell), 1.5f, ORANGE);
    }
    int px = origin_x + (int)(player->x * cell);
    int py = origin_y + (int)(player->y * cell);
    DrawCircle(px, py, 2.0f, SKYBLUE);
    DrawLine(px, py, px + (int)(cosf(player->angle) * 6.0f),
             py + (int)(sinf(player->angle) * 6.0f), SKYBLUE);
    DrawRectangleLines(origin_x - 2, origin_y - 2, width + 4, height + 4, WHITE);
}

static void draw_gun(float recoil_timer, float bob_timer) {
    const float scale = 1.4f;
    float width = hand_gun_texture.width * scale;
    float height = hand_gun_texture.height * scale;
    float x = SCREEN_W / 2.0f - width / 2.0f + 50.0f;
    float y = SCREEN_H - height + 10.0f;
    float recoil = recoil_timer > 0.0f ? recoil_timer / RECOIL_TIME * 14.0f : 0.0f;
    float bob = sinf(bob_timer) * 4.0f;
    DrawTexturePro(hand_gun_texture,
        (Rectangle){0, 0, (float)hand_gun_texture.width, (float)hand_gun_texture.height},
        (Rectangle){x, y + recoil + bob, width, height},
        (Vector2){0, 0}, 0, WHITE);
}

static void draw_hud(Enemy enemies[MAX_ENEMIES], int count, int level) {
    int alive = 0;
    for (int i = 0; i < count; i++) if (enemies[i].state != ENEMY_DEAD) alive++;
    DrawText(TextFormat("%s | Enemigos: %d", LEVEL_NAMES[level], alive), 5, 5, 12, WHITE);
    DrawLine(SCREEN_W / 2 - 5, SCREEN_H / 2, SCREEN_W / 2 + 5, SCREEN_H / 2, WHITE);
    DrawLine(SCREEN_W / 2, SCREEN_H / 2 - 5, SCREEN_W / 2, SCREEN_H / 2 + 5, WHITE);
}

static void draw_centered(const char *text, int y, int size, Color color) {
    int width = MeasureText(text, size);
    DrawText(text, SCREEN_W / 2 - width / 2, y, size, color);
}

static void draw_menu(int selected_level) {
    ClearBackground((Color){15, 15, 25, 255});
    draw_centered("EL MATAKARENS", 38, 34, GOLD);
    draw_centered("Raycaster para PSP", 80, 16, LIGHTGRAY);
    draw_centered("Selecciona un nivel", 116, 16, WHITE);
    draw_centered(TextFormat("<  %s  >", LEVEL_NAMES[selected_level]), 142, 18, YELLOW);
    draw_centered("IZQ/DER: nivel   X/START: jugar", 203, 12, GRAY);
    draw_centered("Analogico: mover/girar  X: disparar", 222, 12, GRAY);
    draw_centered("L/R: desplazarse  SELECT: menu", 240, 12, GRAY);
}

static void draw_success(int level) {
    ClearBackground((Color){10, 30, 15, 255});
    draw_centered("NIVEL COMPLETADO", 80, 28, LIME);
    draw_centered(TextFormat("Terminaste: %s", LEVEL_NAMES[level]), 126, 16, WHITE);
    draw_centered("X o START para volver", 178, 14, GRAY);
}

int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "EL MATAKARENS");
    SetTargetFPS(60);
    init_audio();

    wall_images[0] = load_cpu_png(ASSET_START(wall1_png), ASSET_SIZE(wall1_png));
    wall_images[1] = load_cpu_png(ASSET_START(wall2_png), ASSET_SIZE(wall2_png));
    wall_images[2] = load_cpu_png(ASSET_START(wall3_png), ASSET_SIZE(wall3_png));
    enemy_idle_images[0] = load_cpu_png(ASSET_START(enemy_idle1_png), ASSET_SIZE(enemy_idle1_png));
    enemy_idle_images[1] = load_cpu_png(ASSET_START(enemy_idle2_png), ASSET_SIZE(enemy_idle2_png));
    enemy_hit_image = load_cpu_png(ASSET_START(enemy_hit_png), ASSET_SIZE(enemy_hit_png));
    hand_gun_texture = load_embedded_png(ASSET_START(hand_gun_png), ASSET_SIZE(hand_gun_png));
    Image scene_image = GenImageColor(RENDER_W, RENDER_H, BLACK);
    scene_texture = LoadTextureFromImage(scene_image);
    UnloadImage(scene_image);
    SetTextureFilter(scene_texture, TEXTURE_FILTER_POINT);

    Map map;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    int selected_level = 0;
    int current_level = 0;
    GameState state = STATE_MENU;
    float shoot_cooldown = 0.0f;
    float recoil_timer = 0.0f;
    reset_level(&map, &player, enemies, current_level);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        if (state == STATE_MENU) {
            if (button_pressed(PSP_CTRL_LEFT)) selected_level = (selected_level + 2 - 1) % 2;
            if (button_pressed(PSP_CTRL_RIGHT)) selected_level = (selected_level + 1) % 2;
            if (button_pressed(PSP_CTRL_CROSS) || button_pressed(PSP_CTRL_START)) {
                current_level = selected_level;
                reset_level(&map, &player, enemies, current_level);
                shoot_cooldown = 0.0f;
                recoil_timer = 0.0f;
                state = STATE_PLAYING;
            }
        } else if (state == STATE_PLAYING) {
            if (button_pressed(PSP_CTRL_SELECT)) state = STATE_MENU;
            else {
                update_player(&player, &map, dt);
                update_enemies(enemies, map.enemy_count, dt);
                if (shoot_cooldown > 0.0f) shoot_cooldown -= dt;
                if (recoil_timer > 0.0f) recoil_timer -= dt;
                if (button_pressed(PSP_CTRL_CROSS) && shoot_cooldown <= 0.0f) {
                    shoot_cooldown = SHOOT_COOLDOWN;
                    recoil_timer = RECOIL_TIME;
                    play_gunshot();
                    if (try_shoot(&player, enemies, map.enemy_count, &map)) play_enemy_die();
                }
                if ((int)floorf(player.x) == map.goal_x && (int)floorf(player.y) == map.goal_y)
                    state = STATE_SUCCESS;
            }
        } else if (button_pressed(PSP_CTRL_CROSS) || button_pressed(PSP_CTRL_START)) {
            state = STATE_MENU;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        if (state == STATE_MENU) draw_menu(selected_level);
        else if (state == STATE_PLAYING) {
            render_scene(&map, &player);
            render_enemies(&player, enemies, map.enemy_count);
            UpdateTexture(scene_texture, scene_pixels);
            DrawTexturePro(scene_texture,
                (Rectangle){0, 0, RENDER_W, RENDER_H},
                (Rectangle){0, 0, SCREEN_W, SCREEN_H},
                (Vector2){0, 0}, 0, WHITE);
            draw_minimap(&map, &player, enemies);
            draw_gun(recoil_timer, player.bob_timer);
            draw_hud(enemies, map.enemy_count, current_level);
        } else draw_success(current_level);
        EndDrawing();
    }

    stop_audio();
    for (int i = 0; i < 3; i++) UnloadImageColors(wall_images[i].pixels);
    for (int i = 0; i < 2; i++) UnloadImageColors(enemy_idle_images[i].pixels);
    UnloadImageColors(enemy_hit_image.pixels);
    UnloadTexture(scene_texture);
    UnloadTexture(hand_gun_texture);
    CloseWindow();
    return 0;
}
