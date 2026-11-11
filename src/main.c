#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W 1280
#define H 720
#define MAX_ENEMIES 48
#define MAX_SHOTS 180
#define MAX_PARTICLES 160
#define CHAPTERS 3
#define MAX_UPGRADE 10

typedef enum { MENU, UPGRADE, MISSION, PLAYING, PAUSED, LEVEL_CLEAR, RESULT } Screen;
typedef enum { CAMPAIGN, SURVIVAL } Mode;
typedef enum { KILLS, POINTS, TIME_ALIVE } MissionType;
typedef struct { Vector2 pos, vel; float radius; int damage; bool active, hostile; } Shot;
typedef struct { Vector2 pos; float hp, maxHp, phase, fire; int kind; bool active; } Enemy;
typedef struct { Vector2 pos, vel; float life, maxLife; Color color; } Particle;
typedef struct { MissionType type; int target, reward; bool selected, complete; } Mission;
typedef struct { Vector2 pos; float speed, size; } Star;

static const Color BG = { 6, 12, 30, 255 }, PANEL = { 14, 25, 48, 245 };
static const Color CYAN = { 90, 235, 245, 255 }, MUTED = { 137, 162, 185, 255 };
static const Color WHITE2 = { 230, 244, 248, 255 }, COLOR_PINK = { 255, 105, 142, 255 };
static const Color COLOR_GOLD = { 255, 205, 102, 255 }, COLOR_PURPLE = { 173, 136, 255, 255 };
static Screen screen = MENU;
static Mode mode = CAMPAIGN;
static Shot shots[MAX_SHOTS];
static Enemy enemies[MAX_ENEMIES];
static Particle particles[MAX_PARTICLES];
static Star stars[90];
static Mission mission;
static int menuIndex, credits, weaponLevel = 1, score, lives, chapter, level, wave, kills;
static float playerX, fireTimer, invulnerable, elapsed, banner;
static bool won, missionPaid;

static float randf(float a, float b) { return a + (b - a) * ((float)rand() / RAND_MAX); }
static int randi(int a, int b) { return a + rand() % (b - a + 1); }
static float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static Color alpha(Color c, float a) { c.a = (unsigned char)(clampf(a, 0, 1) * 255); return c; }
static void text(const char *s, int x, int y, int size, Color color) { DrawText(s, x, y, size, color); }
static void center(const char *s, int x, int y, int size, Color color) { text(s, x - MeasureText(s, size)/2, y, size, color); }
static void panel(float x, float y, float w, float h, Color border) {
    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.08f, 8, PANEL);
    DrawRectangleRoundedLinesEx((Rectangle){x, y, w, h}, 0.08f, 8, 1, border);
}
static void saveGame(void) {
    FILE *file = fopen("galaxy_defender.save", "w");
    if (file) { fprintf(file, "%d %d\n", credits, weaponLevel); fclose(file); }
}
static void loadGame(void) {
    FILE *file = fopen("galaxy_defender.save", "r");
    int c, u;
    if (file) {
        if (fscanf(file, "%d %d", &c, &u) == 2 && c >= 0 && c <= 100000000 && u >= 1 && u <= MAX_UPGRADE) { credits = c; weaponLevel = u; }
        fclose(file);
    }
}
static int upgradeCost(void) { return 120 + (weaponLevel - 1) * 100; }
static void makeMission(void) {
    mission.type = (MissionType)randi(0, 2);
    mission.target = mission.type == KILLS ? randi(12, 25) : mission.type == POINTS ? randi(800, 1500) : randi(30, 60);
    mission.reward = randi(140, 320);
    mission.selected = false; mission.complete = false;
}
static const char *missionName(void) {
    if (mission.type == KILLS) return TextFormat("HANCURKAN %d ALIEN", mission.target);
    if (mission.type == POINTS) return TextFormat("RAIH %d POIN", mission.target);
    return TextFormat("BERTAHAN %d DETIK", mission.target);
}
static int missionProgress(void) { return mission.type == KILLS ? kills : mission.type == POINTS ? score : (int)elapsed; }
static void burst(Vector2 pos, Color c, int count) {
    for (int n = 0; n < count; n++) for (int i = 0; i < MAX_PARTICLES; i++) if (particles[i].life <= 0) {
        float a = randf(0, 6.283185f), speed = randf(35, 180);
        particles[i] = (Particle){pos, {cosf(a)*speed, sinf(a)*speed}, randf(.25f, .7f), .7f, c}; break;
    }
}
static void addShot(Vector2 pos, Vector2 vel, int damage, bool hostile, float radius) {
    for (int i = 0; i < MAX_SHOTS; i++) if (!shots[i].active) { shots[i] = (Shot){pos, vel, radius, damage, true, hostile}; return; }
}
static void spawnWave(void) {
    memset(enemies, 0, sizeof enemies); memset(shots, 0, sizeof shots);
    bool boss = mode == CAMPAIGN && level == 10;
    if (boss) {
        float hp = 100.0f + chapter * 80.0f;
        enemies[0] = (Enemy){{W/2.0f, 135}, hp, hp, 0, 1.0f, 2, true};
    } else {
        int difficulty = mode == SURVIVAL ? wave : (chapter - 1)*9 + level;
        int count = 8 + difficulty * 2; if (count > 32) count = 32;
        int cols = 8;
        for (int i = 0; i < count; i++) {
            float hp = 2.0f + (difficulty - 1)/5;
            enemies[i] = (Enemy){{(float)(300 + (i%cols)*96), (float)(110 + (i/cols)*64)}, hp, hp, randf(0, 6), randf(1.0f, 4.0f), i%3 == 0 ? 1 : 0, true};
        }
    }
    banner = 2.6f;
}
static void startGame(Mode selectedMode) {
    mode = selectedMode; chapter = 1; level = 1; wave = 1; score = 0; lives = 3; kills = 0; elapsed = 0;
    playerX = W/2.0f; fireTimer = 0; invulnerable = 0; missionPaid = false; won = false;
    memset(particles, 0, sizeof particles); spawnWave(); screen = PLAYING;
}
static void nextLevel(void) {
    if (mode == SURVIVAL) wave++;
    else if (level == 10) { chapter++; level = 1; }
    else level++;
    spawnWave(); screen = PLAYING;
}
static void damagePlayer(int damage) {
    if (invulnerable > 0) return;
    lives -= damage; if (lives < 0) lives = 0; invulnerable = 1.4f; burst((Vector2){playerX, 630}, CYAN, 18);
    if (lives <= 0) { won = false; saveGame(); screen = RESULT; }
}
static void drawShip(float x, float y, float scale, Color c) {
    DrawTriangle((Vector2){x, y-26*scale}, (Vector2){x-23*scale, y+19*scale}, (Vector2){x+23*scale, y+19*scale}, c);
    DrawTriangle((Vector2){x, y-16*scale}, (Vector2){x-9*scale, y+11*scale}, (Vector2){x+9*scale, y+11*scale}, BG);
    DrawCircleV((Vector2){x, y+1*scale}, 4*scale, WHITE2);
    DrawLineEx((Vector2){x-11*scale, y+23*scale}, (Vector2){x-11*scale, y+32*scale}, 3*scale, COLOR_GOLD);
    DrawLineEx((Vector2){x+11*scale, y+23*scale}, (Vector2){x+11*scale, y+32*scale}, 3*scale, COLOR_GOLD);
}
static void drawEnemy(Enemy *e) {
    float x = e->pos.x, y = e->pos.y;
    if (e->kind == 2) {
        Color c = chapter == 1 ? COLOR_PINK : chapter == 2 ? COLOR_PURPLE : COLOR_GOLD;
        DrawEllipse((int)x, (int)y, 91, 43, alpha(c, .18f));
        DrawTriangle((Vector2){x-105,y-12}, (Vector2){x-67,y+38}, (Vector2){x-25,y+6}, c);
        DrawTriangle((Vector2){x+105,y-12}, (Vector2){x+25,y+6}, (Vector2){x+67,y+38}, c);
        DrawPoly((Vector2){x,y}, 6, 53, 30, c); DrawPoly((Vector2){x,y}, 6, 34, 30, BG);
        DrawCircle((int)x, (int)y, 12, WHITE2); DrawCircle((int)x, (int)y, 6, c);
    } else {
        Color c = e->kind ? COLOR_PURPLE : COLOR_PINK;
        DrawRectangleRounded((Rectangle){x-22,y-13,44,26}, .4f, 4, c);
        DrawTriangle((Vector2){x-22,y}, (Vector2){x-29,y+18}, (Vector2){x-8,y+9}, c);
        DrawTriangle((Vector2){x+22,y}, (Vector2){x+8,y+9}, (Vector2){x+29,y+18}, c);
        DrawRectangle((int)x-12,(int)y-3,7,5,BG); DrawRectangle((int)x+5,(int)y-3,7,5,BG);
    }
}
static void updateGame(float dt) {
    elapsed += dt; banner -= dt; invulnerable -= dt; fireTimer -= dt;
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) { screen = PAUSED; return; }
    float direction = (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) ? 1.0f : 0.0f);
    playerX = clampf(playerX + direction*390*dt, 40, W-40);
    if (IsKeyDown(KEY_SPACE) && fireTimer <= 0) { addShot((Vector2){playerX, 600}, (Vector2){0,-650}, weaponLevel+1, false, 4); fireTimer = .18f; }
    int remaining = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].active) {
        Enemy *e = &enemies[i]; remaining++; e->phase += dt; e->fire -= dt;
        if (e->kind == 2) { e->pos.x = W/2.0f + sinf(e->phase*.7f)*330; e->pos.y = 140 + sinf(e->phase)*20; }
        else { e->pos.x += cosf(e->phase*.9f)*30*dt; e->pos.y += (mode == SURVIVAL ? 2.0f + wave*.25f : 2.0f + chapter)*dt; }
        if (e->pos.y > 580) { e->active = false; damagePlayer(1); if (screen != PLAYING) return; continue; }
        if (e->fire <= 0) {
            if (e->kind == 2) {
                if (chapter == 1) for (int j = -2; j <= 2; j++) addShot(e->pos, (Vector2){j*85.0f, 230}, 2, true, 7);
                else if (chapter == 2) { float dx = playerX-e->pos.x, dy = 630-e->pos.y, len = sqrtf(dx*dx+dy*dy); for (int j = -1; j <= 1; j++) addShot(e->pos, (Vector2){dx/len*260+j*65, dy/len*260}, 2, true, 7); }
                else for (int j = 0; j < 8; j++) { float a = e->phase+j*6.283185f/8; addShot(e->pos, (Vector2){cosf(a)*210,sinf(a)*210}, 2, true, 7); }
                e->fire = chapter == 3 ? .55f : 1.0f;
            } else { addShot(e->pos, (Vector2){0, 180 + chapter*15 + wave*2}, 1, true, 5); e->fire = randf(2.0f, 5.0f); }
        }
    }
    for (int i = 0; i < MAX_SHOTS; i++) if (shots[i].active) {
        Shot *s = &shots[i]; s->pos.x += s->vel.x*dt; s->pos.y += s->vel.y*dt;
        if (s->pos.y < -20 || s->pos.y > H+20 || s->pos.x < -20 || s->pos.x > W+20) { s->active = false; continue; }
        if (s->hostile) {
            if (CheckCollisionCircles(s->pos, s->radius, (Vector2){playerX,630}, 15)) { s->active = false; damagePlayer(s->damage); if (screen != PLAYING) return; }
        } else for (int j = 0; j < MAX_ENEMIES; j++) if (enemies[j].active && CheckCollisionCircles(s->pos, s->radius, enemies[j].pos, enemies[j].kind == 2 ? 45 : 20)) {
            Enemy *e = &enemies[j]; s->active = false; e->hp -= s->damage; burst(s->pos, CYAN, 3);
            if (e->hp <= 0) { int reward = e->kind == 2 ? 150 : 10; score += e->kind == 2 ? 1000 : 100; credits += reward; kills++; burst(e->pos, e->kind == 2 ? COLOR_GOLD : COLOR_PINK, e->kind == 2 ? 35 : 10); e->active = false; remaining--; }
            break;
        }
    }
    if (mission.selected && !missionPaid && missionProgress() >= mission.target) { credits += mission.reward; missionPaid = true; mission.complete = true; mission.selected = false; saveGame(); }
    if (remaining <= 0) {
        saveGame();
        if (mode == CAMPAIGN && chapter == CHAPTERS && level == 10) { won = true; screen = RESULT; }
        else screen = LEVEL_CLEAR;
    }
}

static void background(float dt) {
    DrawRectangleGradientV(0, 0, W, H, (Color){5, 10, 27, 255}, (Color){12, 25, 48, 255});
    DrawCircleGradient(1040, 130, 310, (Color){31, 57, 94, 55}, BLANK);
    DrawCircleGradient(140, 630, 270, (Color){28, 72, 86, 50}, BLANK);
    for (int i = 0; i < 90; i++) {
        stars[i].pos.y += stars[i].speed*dt;
        if (stars[i].pos.y > H) { stars[i].pos.y = 0; stars[i].pos.x = randf(0,W); }
        DrawCircleV(stars[i].pos, stars[i].size, alpha(WHITE2, .25f + stars[i].size*.15f));
    }
    DrawLine(42, 48, W-42, 48, alpha(CYAN, .18f));
    text("G / D", 44, 22, 16, CYAN); text("GALAXY DEFENDER", 110, 22, 14, MUTED);
    text("ARCADE SPACE DEFENSE", W-260, 22, 13, MUTED);
}
static void drawMenu(void) {
    const char *items[] = {"MULAI CAMPAIGN", "SURVIVAL", "UPGRADE SENJATA", "SIDE MISSION", "KELUAR"};
    text("PERTAHANKAN GALAKSI", 92, 110, 15, CYAN);
    text("GALAXY", 86, 150, 76, WHITE2); text("DEFENDER", 86, 224, 76, CYAN);
    text("SATU PESAWAT. SELURUH GALAKSI.", 92, 321, 17, MUTED);
    text("Hancurkan alien, kumpulkan kredit, dan", 92, 365, 19, WHITE2);
    text("bersiap menghadapi boss di setiap chapter.", 92, 392, 19, WHITE2);
    panel(90, 463, 430, 110, alpha(CYAN, .25f));
    text("PERSENJATAAN", 112, 484, 13, MUTED); text(TextFormat("LEVEL %02d", weaponLevel), 112, 510, 26, WHITE2);
    DrawLine(267, 484, 267, 551, alpha(MUTED, .3f));
    text("KREDIT TERSEDIA", 294, 484, 13, MUTED); text(TextFormat("%d CR", credits), 294, 510, 26, COLOR_GOLD);
    panel(680, 102, 510, 489, alpha(CYAN, .24f));
    text("PILIH MISI", 716, 133, 15, CYAN); text("COMMAND CENTER", 716, 160, 25, WHITE2);
    for (int i = 0; i < 5; i++) {
        int y = 218 + i*61;
        if (i == menuIndex) { DrawRectangleRounded((Rectangle){708, y-9, 452, 49}, .18f, 6, alpha(CYAN, .12f)); DrawRectangle(708, y-3, 3, 36, CYAN); }
        text(TextFormat("0%d", i+1), 728, y+3, 14, i == menuIndex ? CYAN : MUTED);
        text(items[i], 779, y, 20, i == menuIndex ? WHITE2 : MUTED);
        if (i == menuIndex) text(">", 1124, y, 20, CYAN);
    }
    text("W/S ATAU PANAH  /  ENTER PILIH", 716, 548, 13, MUTED);
    text("A / D BERGERAK     SPACE MENEMBAK     P JEDA", 90, 664, 14, MUTED);
}
static void drawUpgrade(void) {
    panel(270, 120, 740, 480, alpha(CYAN,.3f));
    text("HANGAR / UPGRADE", 310, 154, 15, CYAN); text("TINGKATKAN SENJATA", 310, 189, 32, WHITE2);
    text("Damage peluru meningkat setiap level.", 310, 235, 18, MUTED);
    drawShip(420, 370, 2.5f, CYAN);
    text("LEVEL SENJATA", 570, 290, 15, MUTED); text(TextFormat("%02d / %02d", weaponLevel, MAX_UPGRADE), 570, 315, 36, WHITE2);
    text(TextFormat("DAMAGE  %d", weaponLevel + 1), 570, 375, 22, CYAN);
    text(TextFormat("KREDIT  %d CR", credits), 570, 411, 20, COLOR_GOLD);
    DrawRectangleRounded((Rectangle){310, 486, 660, 52}, .18f, 6, alpha(CYAN,.12f));
    center(weaponLevel >= MAX_UPGRADE ? "LEVEL MAKSIMUM" : TextFormat("ENTER  /  UPGRADE %d CR", upgradeCost()), 640, 502, 19, weaponLevel >= MAX_UPGRADE ? MUTED : CYAN);
    text("ESC  /  KEMBALI", 310, 556, 14, MUTED);
}
static void drawMission(void) {
    panel(270, 120, 740, 480, alpha(COLOR_PURPLE,.35f));
    text("ARSIP / SIDE MISSION", 310, 154, 15, COLOR_PURPLE); text("KONTRAK SAMPINGAN", 310, 189, 32, WHITE2);
    text("Selesaikan target dalam satu permainan.", 310, 237, 18, MUTED);
    panel(310, 285, 660, 150, alpha(COLOR_PURPLE,.3f));
    text(mission.complete ? "KONTRAK SELESAI" : "TARGET AKTIF", 337, 309, 14, COLOR_PURPLE);
    text(missionName(), 337, 338, 25, WHITE2); text(TextFormat("HADIAH ACAK: %d CR", mission.reward), 337, 383, 18, COLOR_GOLD);
    text(mission.complete ? "ENTER: BUAT MISI BARU" : mission.selected ? "ENTER: BATALKAN PILIHAN" : "ENTER: PILIH MISI", 310, 481, 19, CYAN);
    text(mission.selected && !mission.complete ? "MISI DIPILIH" : "", 310, 516, 15, COLOR_GOLD);
    text("ESC  /  KEMBALI", 310, 556, 14, MUTED);
}
static void drawGame(void) {
    text(mode == CAMPAIGN ? TextFormat("CHAPTER %02d  /  LEVEL %02d", chapter, level) : TextFormat("SURVIVAL  /  WAVE %02d", wave), 48, 70, 18, CYAN);
    text(TextFormat("SCORE  %06d", score), 48, 101, 19, WHITE2);
    text(TextFormat("CREDIT  %d", credits), W-224, 70, 17, COLOR_GOLD);
    text("LIFE", W-224, 103, 15, MUTED);
    for (int i = 0; i < 3; i++) DrawCircle(W-158+i*25, 111, 6, i < lives ? COLOR_PINK : alpha(MUTED,.25f));
    if (mission.selected && !missionPaid) text(TextFormat("MISI: %d / %d", missionProgress() < mission.target ? missionProgress() : mission.target, mission.target), 48, 132, 14, COLOR_PURPLE);
    if (missionPaid) text("MISI SELESAI!", 48, 132, 14, COLOR_GOLD);
    for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].active) {
        Enemy *e = &enemies[i]; drawEnemy(e);
        if (e->kind == 2) { DrawRectangle(390, 69, 500, 8, alpha(MUTED,.25f)); DrawRectangle(390, 69, (int)(500*clampf(e->hp/e->maxHp,0,1)), 8, COLOR_PINK); center("BOSS", 640, 86, 14, COLOR_PINK); }
    }
    for (int i = 0; i < MAX_SHOTS; i++) if (shots[i].active) {
        Shot *s = &shots[i];
        if (s->hostile) { DrawCircleV(s->pos, s->radius+4, alpha(COLOR_PINK,.15f)); DrawCircleV(s->pos, s->radius, COLOR_PINK); }
        else { DrawRectangleRounded((Rectangle){s->pos.x-3,s->pos.y-11,6,20}, .7f, 4, CYAN); }
    }
    if (invulnerable <= 0 || ((int)(invulnerable*10)%2 == 0)) drawShip(playerX, 630, 1, CYAN);
    for (int i = 0; i < MAX_PARTICLES; i++) if (particles[i].life > 0) DrawCircleV(particles[i].pos, 2.5f, alpha(particles[i].color, particles[i].life/particles[i].maxLife));
    text("A / D  GERAK     SPACE  TEMBAK     P  JEDA", 44, 684, 13, MUTED);
    if (banner > 0) {
        DrawRectangle(0, 300, W, 80, alpha(BG,.8f));
        center(mode == SURVIVAL ? TextFormat("WAVE %02d", wave) : level == 10 ? TextFormat("CHAPTER %02d  /  BOSS", chapter) : TextFormat("CHAPTER %02d  /  LEVEL %02d", chapter, level), W/2, 324, 28, WHITE2);
    }
}
static void overlay(const char *eyebrow, const char *title, const char *detail, const char *hint) {
    DrawRectangle(0, 0, W, H, (Color){2,7,18,190}); panel(370, 220, 540, 280, alpha(CYAN,.4f));
    center(eyebrow, W/2, 253, 15, CYAN); center(title, W/2, 291, 38, WHITE2);
    center(detail, W/2, 352, 20, MUTED); center(hint, W/2, 439, 15, COLOR_GOLD);
}
static void updateInput(void) {
    if (screen == MENU) {
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) menuIndex = (menuIndex+1)%5;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) menuIndex = (menuIndex+4)%5;
        if (IsKeyPressed(KEY_ENTER)) {
            if (menuIndex == 0) startGame(CAMPAIGN); else if (menuIndex == 1) startGame(SURVIVAL);
            else if (menuIndex == 2) screen = UPGRADE; else if (menuIndex == 3) screen = MISSION; else CloseWindow();
        }
    } else if (screen == UPGRADE) {
        if (IsKeyPressed(KEY_ESCAPE)) screen = MENU;
        if (IsKeyPressed(KEY_ENTER) && weaponLevel < MAX_UPGRADE && credits >= upgradeCost()) { credits -= upgradeCost(); weaponLevel++; saveGame(); }
    } else if (screen == MISSION) {
        if (IsKeyPressed(KEY_ESCAPE)) screen = MENU;
        if (IsKeyPressed(KEY_ENTER)) { if (mission.complete) makeMission(); else mission.selected = !mission.selected; }
    } else if (screen == PAUSED) {
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ENTER)) screen = PLAYING;
        else if (IsKeyPressed(KEY_ESCAPE)) { saveGame(); screen = MENU; }
    } else if (screen == LEVEL_CLEAR) {
        if (IsKeyPressed(KEY_ENTER)) nextLevel(); else if (IsKeyPressed(KEY_ESCAPE)) screen = MENU;
    } else if (screen == RESULT) {
        if (IsKeyPressed(KEY_ENTER)) { if (mission.complete) makeMission(); startGame(mode); }
        else if (IsKeyPressed(KEY_ESCAPE)) screen = MENU;
    }
}
int main(void) {
    srand((unsigned)time(NULL)); InitWindow(W, H, "Galaxy Defender"); SetExitKey(KEY_NULL); SetTargetFPS(60);
    loadGame(); makeMission();
    for (int i = 0; i < 90; i++) stars[i] = (Star){{randf(0,W),randf(0,H)}, randf(8,35), randf(.5f,2)};
    while (IsWindowReady() && !WindowShouldClose()) {
        float dt = GetFrameTime(); if (dt > .05f) dt = .05f;
        if (screen == PLAYING) updateGame(dt); else updateInput();
        for (int i = 0; i < MAX_PARTICLES; i++) if (particles[i].life > 0 && screen == PLAYING) { particles[i].life -= dt; particles[i].pos.x += particles[i].vel.x*dt; particles[i].pos.y += particles[i].vel.y*dt; }
        if (!IsWindowReady()) break;
        BeginDrawing(); background(dt);
        if (screen == MENU) drawMenu(); else if (screen == UPGRADE) drawUpgrade(); else if (screen == MISSION) drawMission();
        else {
            drawGame();
            if (screen == PAUSED) overlay("PERMAINAN DIJEDA", "PAUSED", "Tarik napas, pilot.", "P / ENTER LANJUT    ESC MENU");
            if (screen == LEVEL_CLEAR) overlay("AREA AMAN", mode == SURVIVAL ? "WAVE SELESAI" : "LEVEL SELESAI", TextFormat("SCORE %d   /   %d CR", score, credits), "ENTER LANJUT    ESC MENU");
            if (screen == RESULT) overlay(won ? "MISI BERHASIL" : "PESAWAT HANCUR", won ? "GALAKSI SELAMAT" : "GAME OVER", TextFormat("SCORE AKHIR %d   /   %d CR", score, credits), "ENTER MAIN LAGI    ESC MENU");
        }
        EndDrawing();
    }
    saveGame(); if (IsWindowReady()) CloseWindow(); return 0;
}
