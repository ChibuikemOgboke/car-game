/* Car Collector v2 - PSP port (C + SDL2)
 * New in v2: main menu, pause menu, game-over menu and an upgrade shop.
 *
 * Controls
 *   D-pad / nub : drive, and move up/down in menus
 *   X (cross)   : select / buy
 *   O (circle)  : back (in the shop)
 *   START       : pause / resume
 *
 * Coins you score each round are added to your "bank". Spend the bank in the
 * shop on permanent upgrades. Progress is saved in progress.txt.
 */
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================= Settings you can tweak ================= */
#define SCREEN_W 480
#define SCREEN_H 272
#define MARGIN 16
#define CAR_W 14
#define CAR_H 25
#define PICKUP_SIZE 24
#define PICKUP_RADIUS 24.0f   /* base pickup distance (Magnet upgrade adds to it) */
#define PLAYER_SPEED 170.0f   /* base speed (Engine upgrade adds to it) */
#define START_TIME 30.0f      /* base time (Big Tank upgrade adds to it) */
#define FONT_SIZE 16
#define TITLE_SIZE 30

/* PSP button numbers as SDL sees them */
#define BTN_CIRCLE 1
#define BTN_CROSS  2
#define BTN_DOWN   6
#define BTN_LEFT   7
#define BTN_UP     8
#define BTN_RIGHT  9
#define BTN_START  11
#define AXIS_DEADZONE 16000

/* ================= Paths & logging ================= */
static char base_dir[256] = "";
static FILE *log_file = NULL;

#define LOG(...) do { fprintf(stderr, __VA_ARGS__); \
    if (log_file) { fprintf(log_file, __VA_ARGS__); fflush(log_file); } } while (0)

/* Assets and save files live next to EBOOT.PBP. */
static void init_base_dir(const char *argv0) {
    if (!argv0) return;
    snprintf(base_dir, sizeof base_dir, "%s", argv0);
    char *slash = strrchr(base_dir, '/');
    char *bslash = strrchr(base_dir, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
    if (slash) slash[1] = '\0'; else base_dir[0] = '\0';
}

static const char *P(const char *rel) {
    static char buf[4][512];
    static int i = 0;
    i = (i + 1) & 3;
    snprintf(buf[i], sizeof buf[i], "%s%s", base_dir, rel);
    return buf[i];
}

/* ================= Text drawing (with a small cache) =================
 * Making a text texture is slow, so we remember recent ones. You can just
 * call draw_text() every frame and not worry about it. */
enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

typedef struct {            /* a struct is like a tiny class with only variables */
    TTF_Font *font;
    SDL_Color col;
    char text[64];
    SDL_Texture *tex;
    int w, h;
    unsigned last_used;
} TextEntry;

#define TEXT_CACHE_SIZE 48
static TextEntry text_cache[TEXT_CACHE_SIZE];
static SDL_Renderer *renderer;

static TextEntry *get_text(TTF_Font *font, const char *text, SDL_Color col) {
    static unsigned tick = 0;
    tick++;
    int victim = 0;   /* the least recently used slot gets replaced */
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        TextEntry *e = &text_cache[i];
        if (e->tex && e->font == font && e->col.r == col.r && e->col.g == col.g &&
            e->col.b == col.b && strcmp(e->text, text) == 0) {
            e->last_used = tick;
            return e;
        }
        if (e->last_used < text_cache[victim].last_used) victim = i;
    }
    TextEntry *e = &text_cache[victim];
    if (e->tex) SDL_DestroyTexture(e->tex);
    e->tex = NULL;
    snprintf(e->text, sizeof e->text, "%s", text);
    e->font = font;
    e->col = col;
    e->last_used = tick;
    SDL_Surface *s = TTF_RenderText_Blended(font, e->text, col);
    if (s) {
        e->tex = SDL_CreateTextureFromSurface(renderer, s);
        e->w = s->w;
        e->h = s->h;
        SDL_FreeSurface(s);
    }
    return e;
}

/* x is the left edge, centre, or right edge depending on align; y is the middle */
static void draw_text(TTF_Font *font, const char *text, SDL_Color col, int x, int y, int align) {
    TextEntry *t = get_text(font, text, col);
    if (!t->tex) return;
    int left = x;
    if (align == ALIGN_CENTER) left = x - t->w / 2;
    else if (align == ALIGN_RIGHT) left = x - t->w;
    SDL_Rect r = { left, y - t->h / 2, t->w, t->h };
    SDL_RenderCopy(renderer, t->tex, NULL, &r);
}

/* ================= Small helpers ================= */
static const SDL_Color COL_WHITE  = {255, 255, 255, 255};
static const SDL_Color COL_YELLOW = {255, 255, 0, 255};
static const SDL_Color COL_CYAN   = {0, 255, 255, 255};
static const SDL_Color COL_RED    = {255, 70, 70, 255};
static const SDL_Color COL_GRAY   = {150, 150, 150, 255};

static float frand(float lo, float hi) {
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}
static float rand_x(void) { return frand(MARGIN, SCREEN_W - MARGIN); }
static float rand_y(void) { return frand(MARGIN, SCREEN_H - MARGIN); }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float dist(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return SDL_sqrtf(dx * dx + dy * dy);
}

static SDL_Texture *load_tex(const char *path) {
    SDL_Texture *t = IMG_LoadTexture(renderer, path);
    if (!t) LOG("Failed to load %s: %s\n", path, IMG_GetError());
    return t;
}

/* ================= Upgrades ================= */
enum { UP_ENGINE, UP_MAGNET, UP_TANK, UP_COIN, NUM_UPGRADES };

typedef struct {
    const char *name;
    const char *effect;   /* what each level gives you */
    int max_level;
    int base_cost;        /* level 0 -> 1 costs base_cost, then 2x, 3x, ... */
} Upgrade;

static const Upgrade upgrades[NUM_UPGRADES] = {
    { "Engine",     "+12% speed",        5, 20 },
    { "Magnet",     "+4 pickup range",   5, 20 },
    { "Big Tank",   "+5s start time",    5, 25 },
    { "Coin Value", "+1 per coin",       3, 40 },
};

static int upgrade_level[NUM_UPGRADES];   /* all start at 0 */
static int bank = 0;                      /* coins you can spend */

static int upgrade_cost(int i) { return upgrades[i].base_cost * (upgrade_level[i] + 1); }

/* The actual numbers the game uses this round, worked out from the levels */
static float stat_speed, stat_radius, stat_start_time;
static int stat_coin_value;

static void apply_upgrades(void) {
    stat_speed      = PLAYER_SPEED  * (1.0f + 0.12f * upgrade_level[UP_ENGINE]);
    stat_radius     = PICKUP_RADIUS + 4.0f * upgrade_level[UP_MAGNET];
    stat_start_time = START_TIME    + 5.0f * upgrade_level[UP_TANK];
    stat_coin_value = 1 + upgrade_level[UP_COIN];
}

/* ================= Saving ================= */
static int load_high_score(void) {
    FILE *f = fopen(P("highscore.txt"), "r");
    int v = 0;
    if (f) {
        if (fscanf(f, "%d", &v) != 1) v = 0;
        fclose(f);
    }
    return v < 0 ? 0 : v;
}

static void save_high_score(int v) {
    FILE *f = fopen(P("highscore.txt"), "w");
    if (f) {
        fprintf(f, "%d", v);
        fclose(f);
    } else {
        LOG("Could not save high score (errno %d)\n", errno);
    }
}

/* progress.txt holds:  bank level0 level1 level2 level3 */
static void load_progress(void) {
    FILE *f = fopen(P("progress.txt"), "r");
    if (!f) return;
    int b;
    if (fscanf(f, "%d", &b) == 1) {
        bank = b < 0 ? 0 : b;
        for (int i = 0; i < NUM_UPGRADES; i++) {
            int v;
            if (fscanf(f, " %d", &v) != 1) break;
            upgrade_level[i] = clampi(v, 0, upgrades[i].max_level);
        }
    }
    fclose(f);
}

static void save_progress(void) {
    FILE *f = fopen(P("progress.txt"), "w");
    if (!f) {
        LOG("Could not save progress (errno %d)\n", errno);
        return;
    }
    fprintf(f, "%d", bank);
    for (int i = 0; i < NUM_UPGRADES; i++) fprintf(f, " %d", upgrade_level[i]);
    fclose(f);
}

/* ================= Game state ================= */
typedef enum { STATE_MENU, STATE_SHOP, STATE_PLAYING, STATE_PAUSED, STATE_GAME_OVER } GameState;
static GameState state = STATE_MENU;

static int menu_sel = 0, shop_sel = 0, pause_sel = 0, over_sel = 0;   /* highlighted item */
static char shop_msg[32] = "";
static float shop_msg_timer = 0.0f;

static int score = 0, high_score = 0, round_earned = 0;
static float time_left = START_TIME;
static float px = SCREEN_W / 2.0f, py = SCREEN_H / 2.0f;
static float car_angle = 0.0f;
static float coin_x, coin_y;
static float jerry_x, jerry_y, jerry_timer;
static float gem_x, gem_y, gem_timer;
static int jerry_visible = 0, gem_visible = 0;
static int game_over_sound_played = 0;

static SDL_Texture *car_tex, *coin_tex, *jerry_tex, *gem_tex;
static Mix_Music *music;
static Mix_Chunk *snd_coin, *snd_jerry, *snd_bonus, *snd_gameover;
static TTF_Font *font, *title_font;

static void reset_game(void) {
    apply_upgrades();
    score = 0;
    time_left = stat_start_time;
    px = SCREEN_W / 2.0f;
    py = SCREEN_H / 2.0f;
    car_angle = 0.0f;
    coin_x = rand_x();
    coin_y = rand_y();
    jerry_visible = 0;
    gem_visible = 0;
    jerry_timer = frand(5.0f, 10.0f);
    gem_timer = frand(3.0f, 6.0f);
    game_over_sound_played = 0;
    Mix_HaltChannel(-1);
    if (music) Mix_PlayMusic(music, -1);
}

static void start_game(void) {
    reset_game();
    state = STATE_PLAYING;
}

static void go_to_menu(void) {
    Mix_HaltMusic();
    state = STATE_MENU;
    menu_sel = 0;
}

static void go_to_shop(void) {
    Mix_HaltMusic();
    state = STATE_SHOP;
    shop_sel = 0;
    shop_msg_timer = 0.0f;
}

static void end_round(void) {
    time_left = 0.0f;
    round_earned = score;
    bank += score;            /* the score you earned becomes spendable coins */
    save_progress();
    over_sel = 0;
    state = STATE_GAME_OVER;
}

static void add_score(int n) {
    score += n;
    if (score > high_score) {
        high_score = score;
        save_high_score(high_score);
    }
}

static void set_shop_msg(const char *msg) {
    snprintf(shop_msg, sizeof shop_msg, "%s", msg);
    shop_msg_timer = 1.5f;
}

static void try_buy(int i) {
    if (upgrade_level[i] >= upgrades[i].max_level) { set_shop_msg("Already maxed out"); return; }
    int cost = upgrade_cost(i);
    if (bank < cost) { set_shop_msg("Not enough coins"); return; }
    bank -= cost;
    upgrade_level[i]++;
    save_progress();
    set_shop_msg("Upgraded!");
}

/* Move a menu highlight up or down, wrapping around at the ends */
static int nav(int sel, int count, int up_pressed, int down_pressed) {
    if (up_pressed)   sel = (sel + count - 1) % count;
    if (down_pressed) sel = (sel + 1) % count;
    return sel;
}

/* ================= Input ================= */
typedef struct {
    int up, down, left, right;                 /* held right now */
    int up_p, down_p, cross_p, circle_p, start_p;   /* pressed THIS frame only */
} Input;

static Input read_input(SDL_Joystick *js) {
    static int p_up, p_down, p_cross, p_circle, p_start;   /* last frame's values */
    int up = 0, down = 0, left = 0, right = 0, cross = 0, circle = 0, start = 0;

    if (js) {
        cross  = SDL_JoystickGetButton(js, BTN_CROSS);
        circle = SDL_JoystickGetButton(js, BTN_CIRCLE);
        start  = SDL_JoystickGetButton(js, BTN_START);
        up     = SDL_JoystickGetButton(js, BTN_UP);
        down   = SDL_JoystickGetButton(js, BTN_DOWN);
        left   = SDL_JoystickGetButton(js, BTN_LEFT);
        right  = SDL_JoystickGetButton(js, BTN_RIGHT);
        int ax = SDL_JoystickGetAxis(js, 0);
        int ay = SDL_JoystickGetAxis(js, 1);
        if (ax < -AXIS_DEADZONE) left = 1;
        if (ax >  AXIS_DEADZONE) right = 1;
        if (ay < -AXIS_DEADZONE) up = 1;
        if (ay >  AXIS_DEADZONE) down = 1;
    }
    /* keyboard, handy for PC builds */
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    if (kb[SDL_SCANCODE_RETURN] || kb[SDL_SCANCODE_SPACE]) cross = 1;
    if (kb[SDL_SCANCODE_BACKSPACE] || kb[SDL_SCANCODE_B]) circle = 1;
    if (kb[SDL_SCANCODE_ESCAPE] || kb[SDL_SCANCODE_P]) start = 1;
    if (kb[SDL_SCANCODE_W] || kb[SDL_SCANCODE_UP]) up = 1;
    if (kb[SDL_SCANCODE_S] || kb[SDL_SCANCODE_DOWN]) down = 1;
    if (kb[SDL_SCANCODE_A] || kb[SDL_SCANCODE_LEFT]) left = 1;
    if (kb[SDL_SCANCODE_D] || kb[SDL_SCANCODE_RIGHT]) right = 1;

    Input inp;
    inp.up = up; inp.down = down; inp.left = left; inp.right = right;
    inp.up_p = up && !p_up;
    inp.down_p = down && !p_down;
    inp.cross_p = cross && !p_cross;
    inp.circle_p = circle && !p_circle;
    inp.start_p = start && !p_start;
    p_up = up; p_down = down; p_cross = cross; p_circle = circle; p_start = start;
    return inp;
}

/* ================= Gameplay update ================= */
static void update_playing(float dt, const Input *inp) {
    time_left -= dt;
    if (time_left <= 0.0f) {
        end_round();
        return;
    }

    int dx = inp->right - inp->left;
    int dy = inp->down - inp->up;
    float spd = stat_speed * dt;
    if (dx && dy) spd *= 0.707f;     /* diagonals are slower so they aren't faster overall */
    px += dx * spd;
    py += dy * spd;

    /* The car faces the way it moves (the sprite points down at angle 0) */
    if      (dx ==  0 && dy ==  1) car_angle = 0.0f;
    else if (dx == -1 && dy ==  1) car_angle = 45.0f;
    else if (dx == -1 && dy ==  0) car_angle = 90.0f;
    else if (dx == -1 && dy == -1) car_angle = 135.0f;
    else if (dx ==  0 && dy == -1) car_angle = 180.0f;
    else if (dx ==  1 && dy == -1) car_angle = 225.0f;
    else if (dx ==  1 && dy ==  0) car_angle = 270.0f;
    else if (dx ==  1 && dy ==  1) car_angle = 315.0f;

    px = clampf(px, MARGIN + CAR_W, SCREEN_W - MARGIN - CAR_W);
    py = clampf(py, MARGIN + CAR_H / 2, SCREEN_H - MARGIN - CAR_H / 2);

    /* Coin */
    if (dist(px, py, coin_x, coin_y) < stat_radius) {
        coin_x = rand_x();
        coin_y = rand_y();
        if (snd_coin) Mix_PlayChannel(-1, snd_coin, 0);
        add_score(stat_coin_value);
    }

    /* Gem (+5) */
    if (!gem_visible) {
        gem_timer -= dt;
        if (gem_timer <= 0.0f) {
            gem_x = rand_x();
            gem_y = rand_y();
            gem_visible = 1;
        }
    } else if (dist(px, py, gem_x, gem_y) < stat_radius) {
        if (snd_bonus) Mix_PlayChannel(-1, snd_bonus, 0);
        gem_visible = 0;
        gem_timer = frand(3.0f, 6.0f);
        add_score(5);
    }

    /* Jerry can (+8 seconds) */
    if (!jerry_visible) {
        jerry_timer -= dt;
        if (jerry_timer <= 0.0f) {
            jerry_x = rand_x();
            jerry_y = rand_y();
            jerry_visible = 1;
        }
    } else if (dist(px, py, jerry_x, jerry_y) < stat_radius) {
        if (snd_jerry) Mix_PlayChannel(-1, snd_jerry, 0);
        time_left += 8.0f;
        jerry_visible = 0;
        jerry_timer = frand(5.0f, 10.0f);
    }
}

/* ================= Drawing ================= */
static void clear_screen(Uint8 r, Uint8 g, Uint8 b) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderClear(renderer);
}

/* items[] is a list of strings; the selected one is highlighted */
static void draw_menu_items(const char *const items[], int count, int sel, int y0) {
    char buf[48];
    for (int i = 0; i < count; i++) {
        int y = y0 + i * 30;
        if (i == sel) {
            snprintf(buf, sizeof buf, "> %s <", items[i]);
            draw_text(font, buf, COL_YELLOW, SCREEN_W / 2, y, ALIGN_CENTER);
        } else {
            draw_text(font, items[i], COL_WHITE, SCREEN_W / 2, y, ALIGN_CENTER);
        }
    }
}

static void draw_main_menu(void) {
    static const char *const items[] = { "Play", "Upgrades" };
    char buf[48];
    clear_screen(30, 30, 45);
    draw_text(title_font, "CAR COLLECTOR", COL_CYAN, SCREEN_W / 2, 60, ALIGN_CENTER);
    draw_menu_items(items, 2, menu_sel, 130);
    snprintf(buf, sizeof buf, "High Score: %d", high_score);
    draw_text(font, buf, COL_YELLOW, SCREEN_W / 2, 215, ALIGN_CENTER);
    snprintf(buf, sizeof buf, "Coins: %d", bank);
    draw_text(font, buf, COL_WHITE, SCREEN_W / 2, 240, ALIGN_CENTER);
}

static void draw_shop(void) {
    char buf[48];
    clear_screen(30, 30, 45);
    draw_text(title_font, "UPGRADES", COL_CYAN, SCREEN_W / 2, 28, ALIGN_CENTER);
    snprintf(buf, sizeof buf, "Coins: %d", bank);
    draw_text(font, buf, COL_YELLOW, SCREEN_W / 2, 65, ALIGN_CENTER);

    for (int i = 0; i < NUM_UPGRADES; i++) {
        int y = 100 + i * 30;
        int maxed = upgrade_level[i] >= upgrades[i].max_level;
        int affordable = bank >= upgrade_cost(i);
        SDL_Color c = (i == shop_sel) ? COL_YELLOW : COL_WHITE;

        if (i == shop_sel) draw_text(font, ">", c, 14, y, ALIGN_LEFT);
        draw_text(font, upgrades[i].name, c, 30, y, ALIGN_LEFT);
        snprintf(buf, sizeof buf, "Lv %d/%d", upgrade_level[i], upgrades[i].max_level);
        draw_text(font, buf, c, 150, y, ALIGN_LEFT);
        draw_text(font, upgrades[i].effect, COL_GRAY, 225, y, ALIGN_LEFT);
        if (maxed) {
            draw_text(font, "MAX", COL_CYAN, SCREEN_W - 15, y, ALIGN_RIGHT);
        } else {
            snprintf(buf, sizeof buf, "Cost %d", upgrade_cost(i));
            draw_text(font, buf, affordable ? c : COL_RED, SCREEN_W - 15, y, ALIGN_RIGHT);
        }
    }

    if (shop_msg_timer > 0.0f)
        draw_text(font, shop_msg, COL_CYAN, SCREEN_W / 2, 220, ALIGN_CENTER);
    draw_text(font, "X: buy     O: back", COL_GRAY, SCREEN_W / 2, 250, ALIGN_CENTER);
}

static void draw_world(void) {
    clear_screen(128, 0, 128);

    SDL_Rect r = { (int)coin_x - PICKUP_SIZE / 2, (int)coin_y - PICKUP_SIZE / 2,
                   PICKUP_SIZE, PICKUP_SIZE };
    if (coin_tex) SDL_RenderCopy(renderer, coin_tex, NULL, &r);
    if (gem_visible && gem_tex) {
        r.x = (int)gem_x - PICKUP_SIZE / 2;
        r.y = (int)gem_y - PICKUP_SIZE / 2;
        SDL_RenderCopy(renderer, gem_tex, NULL, &r);
    }
    if (jerry_visible && jerry_tex) {
        r.x = (int)jerry_x - PICKUP_SIZE / 2;
        r.y = (int)jerry_y - PICKUP_SIZE / 2;
        SDL_RenderCopy(renderer, jerry_tex, NULL, &r);
    }
    if (car_tex) {
        SDL_Rect cr = { (int)px - CAR_W / 2, (int)py - CAR_H / 2, CAR_W, CAR_H };
        SDL_RenderCopyEx(renderer, car_tex, NULL, &cr, car_angle, NULL, SDL_FLIP_NONE);
    }

    char buf[48];
    snprintf(buf, sizeof buf, "Score: %d", score);
    draw_text(font, buf, COL_WHITE, 10, 14, ALIGN_LEFT);
    snprintf(buf, sizeof buf, "High Score: %d", high_score);
    draw_text(font, buf, COL_YELLOW, 10, 34, ALIGN_LEFT);
    snprintf(buf, sizeof buf, "Time: %ds", (int)time_left);
    draw_text(font, buf, COL_WHITE, SCREEN_W - 10, 14, ALIGN_RIGHT);
}

static void draw_pause(void) {
    static const char *const items[] = { "Resume", "Restart", "Main Menu" };
    SDL_Rect full = { 0, 0, SCREEN_W, SCREEN_H };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
    SDL_RenderFillRect(renderer, &full);
    draw_text(title_font, "PAUSED", COL_WHITE, SCREEN_W / 2, 70, ALIGN_CENTER);
    draw_menu_items(items, 3, pause_sel, 125);
}

static void draw_game_over(void) {
    static const char *const items[] = { "Play Again", "Upgrades", "Main Menu" };
    char buf[48];
    clear_screen(0, 0, 0);
    draw_text(title_font, "GAME OVER", COL_RED, SCREEN_W / 2, 35, ALIGN_CENTER);
    snprintf(buf, sizeof buf, "Final Score: %d", score);
    draw_text(font, buf, COL_WHITE, SCREEN_W / 2, 80, ALIGN_CENTER);
    snprintf(buf, sizeof buf, "High Score: %d", high_score);
    draw_text(font, buf, COL_YELLOW, SCREEN_W / 2, 102, ALIGN_CENTER);
    snprintf(buf, sizeof buf, "+%d coins banked (total %d)", round_earned, bank);
    draw_text(font, buf, COL_CYAN, SCREEN_W / 2, 124, ALIGN_CENTER);
    draw_menu_items(items, 3, over_sel, 170);
}

/* ================= Main ================= */
int main(int argc, char *argv[]) {
    if (argc > 0) init_base_dir(argv[0]);
    log_file = fopen(P("log.txt"), "w");
    LOG("Car Collector v2 starting. base dir: '%s'\n", base_dir);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        LOG("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
        LOG("Mix_OpenAudio: %s\n", Mix_GetError());

    SDL_Window *window = SDL_CreateWindow("Car Collector", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, SCREEN_W, SCREEN_H, 0);
    if (window) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (!window || !renderer) {
        LOG("Window/renderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Joystick *js = NULL;
    if (SDL_NumJoysticks() > 0) js = SDL_JoystickOpen(0);

    /* Images must be 512x512 or smaller on the PSP */
    car_tex   = load_tex(P("assets/car.png"));
    coin_tex  = load_tex(P("assets/coin.png"));
    jerry_tex = load_tex(P("assets/jerrycan.png"));
    gem_tex   = load_tex(P("assets/gem.png"));

    music        = Mix_LoadMUS(P("assets/music1.mp3"));
    snd_jerry    = Mix_LoadWAV(P("assets/ding.wav"));
    snd_coin     = Mix_LoadWAV(P("assets/coin.wav"));
    snd_bonus    = Mix_LoadWAV(P("assets/bonus.wav"));
    snd_gameover = Mix_LoadWAV(P("assets/gameover.wav"));
    if (!music) LOG("Music: %s\n", Mix_GetError());

    Mix_VolumeMusic((int)(0.4f * MIX_MAX_VOLUME));
    Mix_Chunk *chunks[] = { snd_jerry, snd_coin, snd_bonus, snd_gameover };
    for (int i = 0; i < 4; i++)
        if (chunks[i]) Mix_VolumeChunk(chunks[i], (int)(0.6f * MIX_MAX_VOLUME));

    font       = TTF_OpenFont(P("assets/font.ttf"), FONT_SIZE);
    title_font = TTF_OpenFont(P("assets/font.ttf"), TITLE_SIZE);
    if (!font || !title_font) {
        LOG("Font: %s (put a .ttf at assets/font.ttf)\n", TTF_GetError());
        return 1;
    }

    high_score = load_high_score();
    load_progress();
    apply_upgrades();

    int running = 1;
    Uint32 last = SDL_GetTicks();

    while (running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;   /* seconds since last frame */
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        Input inp = read_input(js);

        /* ---- Menus and state changes ---- */
        switch (state) {
        case STATE_MENU:
            menu_sel = nav(menu_sel, 2, inp.up_p, inp.down_p);
            if (inp.cross_p) {
                if (menu_sel == 0) start_game();
                else go_to_shop();
            }
            break;

        case STATE_SHOP:
            shop_sel = nav(shop_sel, NUM_UPGRADES, inp.up_p, inp.down_p);
            if (inp.cross_p) try_buy(shop_sel);
            if (inp.circle_p) go_to_menu();
            if (shop_msg_timer > 0.0f) shop_msg_timer -= dt;
            break;

        case STATE_PLAYING:
            if (inp.start_p) {
                state = STATE_PAUSED;
                pause_sel = 0;
                Mix_PauseMusic();
            }
            break;

        case STATE_PAUSED:
            pause_sel = nav(pause_sel, 3, inp.up_p, inp.down_p);
            if (inp.start_p || (inp.cross_p && pause_sel == 0)) {
                state = STATE_PLAYING;
                Mix_ResumeMusic();
            } else if (inp.cross_p && pause_sel == 1) {
                start_game();
            } else if (inp.cross_p && pause_sel == 2) {
                go_to_menu();
            }
            break;

        case STATE_GAME_OVER:
            over_sel = nav(over_sel, 3, inp.up_p, inp.down_p);
            if (inp.cross_p) {
                if (over_sel == 0) start_game();
                else if (over_sel == 1) go_to_shop();
                else go_to_menu();
            }
            break;
        }

        /* ---- Update ---- */
        if (state == STATE_PLAYING) update_playing(dt, &inp);

        if (state == STATE_GAME_OVER && !game_over_sound_played) {
            Mix_PauseMusic();
            if (snd_gameover) Mix_PlayChannel(-1, snd_gameover, 0);
            game_over_sound_played = 1;
        }

        /* ---- Draw ---- */
        switch (state) {
        case STATE_MENU:      draw_main_menu(); break;
        case STATE_SHOP:      draw_shop(); break;
        case STATE_GAME_OVER: draw_game_over(); break;
        case STATE_PLAYING:   draw_world(); break;
        case STATE_PAUSED:    draw_world(); draw_pause(); break;
        }

        SDL_RenderPresent(renderer);
    }

    Mix_HaltMusic();
    if (js) SDL_JoystickClose(js);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
