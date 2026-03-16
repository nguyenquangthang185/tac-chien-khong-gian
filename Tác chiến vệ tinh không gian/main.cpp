/*
 * main_upgrade_pack2_random_targets.cpp
 * Upgrade Pack 2:
 * - Booster flame dep hon theo tung phase
 * - Shockwave mat dat
 * - Bui no lan ngang
 * - Reset ngau nhien so luong muc tieu va hinh dang
 */

#include <GL/freeglut.h>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int SCREEN_WIDTH = 1400;
const int SCREEN_HEIGHT = 900;
const char* WINDOW_TITLE = "Recon Satellite & Launcher - Upgrade Pack 2";

struct Vec2 {
    float x, y;
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float k) const { return {x * k, y * k}; }
};

float length(const Vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
float distance2D(const Vec2& a, const Vec2& b) { return length(a - b); }
float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }
float randFloat(float a, float b) { return a + (b - a) * (float)std::rand() / (float)RAND_MAX; }
int randInt(int a, int b) { return a + std::rand() % (b - a + 1); }
bool pointInRect(float px, float py, float x1, float y1, float x2, float y2) { return px >= x1 && px <= x2 && py >= y1 && py <= y2; }

struct Star { float x, y, phase, speed; };
struct Smoke { Vec2 pos, vel; float life, size; };
struct Trail { Vec2 pos; float life, size; };
struct Debris { Vec2 pos, vel; float life, size, rot, rotSpeed; };
struct Notification { std::string text; float timer, life; int type; };
struct GroundShockwave { Vec2 pos; float radius; float alpha; bool active; };
struct DustWave { Vec2 pos, vel; float life, size; };

enum TargetShape { TARGET_SQUARE, TARGET_DIAMOND, TARGET_TRIANGLE, TARGET_HEX };

struct SatelliteRecon {
    Vec2 center = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f + 20.0f};
    float orbitRadius = 270.0f;
    float angle = 0.0f;
    float orbitSpeed = 0.16f;
    float pulse = 0.0f;
    Vec2 position() const { return {center.x + std::cos(angle) * orbitRadius, center.y + std::sin(angle) * orbitRadius}; }
    void update(float dt) {
        angle += orbitSpeed * dt;
        pulse += dt * 3.8f;
        if (angle > 2 * M_PI) angle -= 2 * M_PI;
        if (pulse > 2 * M_PI) pulse -= 2 * M_PI;
    }
};

struct Target {
    Vec2 basePos;
    Vec2 pos;
    float size;
    float phase;
    bool locked = false;
    bool destroyed = false;
    int id;
    TargetShape shape = TARGET_SQUARE;
    void update(float t) {
        if (destroyed) return;
        pos.x = basePos.x + std::cos(t * 0.75f + phase) * 14.0f;
        pos.y = basePos.y + std::sin(t * 1.05f + phase) * 10.0f;
    }
};

struct LauncherVehicle {
    Vec2 pos = {180, 125};
    float erectProgress = 0.0f;
    float recoil = 0.0f;
    float hatchOpen = 0.0f;
    float warningPulse = 0.0f;
    Vec2 missileBasePos() const { return {pos.x + 40.0f, pos.y + 48.0f}; }
};

struct BallisticMissile {
    Vec2 pos, vel, startPos, targetPos;
    bool active = false;
    float angleDeg = 90.0f;
    float gravity = 260.0f;
    float age = 0.0f;
    float booster = 1.0f;
};

struct Explosion {
    Vec2 pos;
    bool active = false;
    float time = 0.0f;
    float duration = 3.2f;
    float radius = 30.0f;
};

enum AppMode { MODE_START_SCREEN, MODE_COMMAND_SCREEN, MODE_SIM_RUNNING, MODE_MISSION_COMPLETE };
enum SeqState { SEQ_IDLE, SEQ_TARGET_LOCKED, SEQ_COUNTDOWN, SEQ_LAUNCH_PREP, SEQ_ASCENT, SEQ_PITCHOVER, SEQ_BALLISTIC, SEQ_IMPACT };

SatelliteRecon g_satellite;
LauncherVehicle g_launcher;
BallisticMissile g_missile;
Explosion g_explosion;
std::vector<Target> g_targets;
std::vector<Star> g_stars;
std::vector<Smoke> g_smoke;
std::vector<Trail> g_trail;
std::vector<Debris> g_debris;
std::vector<Notification> g_notifications;
std::vector<GroundShockwave> g_groundShockwaves;
std::vector<DustWave> g_dustWaves;

AppMode g_appMode = MODE_START_SCREEN;
SeqState g_seqState = SEQ_IDLE;
int g_selectedTarget = -1;
int g_score = 0;
int g_destroyedCount = 0;
int g_missileCount = 0;
float g_lastTime = 0.0f;
float g_worldTime = 0.0f;
float g_uiPulse = 0.0f;
float g_radarSweep = 0.0f;
float g_launchTimer = 0.0f;
float g_countdownTimer = 0.0f;
float g_screenFlash = 0.0f;
float g_cameraShake = 0.0f;
float g_fps = 0.0f;
float g_fpsTimer = 0.0f;
int g_frameCount = 0;
Vec2 g_mousePos;
bool g_showGrid = true;
bool g_showMissionPanel = true;
bool g_showMinimap = true;

void drawFilledRect(float x1, float y1, float x2, float y2) {
    glBegin(GL_QUADS);
    glVertex2f(x1, y1); glVertex2f(x2, y1); glVertex2f(x2, y2); glVertex2f(x1, y2);
    glEnd();
}

void drawText(float x, float y, const std::string& s, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (char c : s) glutBitmapCharacter(font, c);
}

void drawTextCentered(float cx, float y, const std::string& s, void* font = GLUT_BITMAP_HELVETICA_18) {
    int w = 0; for (char c : s) w += glutBitmapWidth(font, c);
    glRasterPos2f(cx - w / 2.0f, y);
    for (char c : s) glutBitmapCharacter(font, c);
}

void drawCircle(float cx, float cy, float r, int segments = 40, bool filled = true) {
    glBegin(filled ? GL_POLYGON : GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * M_PI * i / segments;
        glVertex2f(cx + std::cos(a) * r, cy + std::sin(a) * r);
    }
    glEnd();
}

void drawPolyShape(TargetShape shape, float size, bool filled = false) {
    GLenum mode = filled ? GL_POLYGON : GL_LINE_LOOP;
    glBegin(mode);
    if (shape == TARGET_SQUARE) {
        glVertex2f(-size, -size); glVertex2f(size, -size); glVertex2f(size, size); glVertex2f(-size, size);
    } else if (shape == TARGET_DIAMOND) {
        glVertex2f(0, -size); glVertex2f(size, 0); glVertex2f(0, size); glVertex2f(-size, 0);
    } else if (shape == TARGET_TRIANGLE) {
        glVertex2f(0, size); glVertex2f(size, -size); glVertex2f(-size, -size);
    } else {
        for (int i = 0; i < 6; ++i) {
            float a = 2.0f * M_PI * i / 6.0f + M_PI / 6.0f;
            glVertex2f(std::cos(a) * size, std::sin(a) * size);
        }
    }
    glEnd();
}

void drawRoundedRect(float x1, float y1, float x2, float y2, float r, bool filled = true) {
    const int seg = 14;
    if (filled) {
        drawFilledRect(x1 + r, y1, x2 - r, y2);
        drawFilledRect(x1, y1 + r, x2, y2 - r);
    }
    for (int c = 0; c < 4; ++c) {
        float cx = (c == 0 || c == 3) ? x1 + r : x2 - r;
        float cy = (c == 0 || c == 1) ? y1 + r : y2 - r;
        float start = 0.0f;
        if (c == 0) start = M_PI;
        if (c == 1) start = -M_PI / 2.0f;
        if (c == 2) start = 0.0f;
        if (c == 3) start = M_PI / 2.0f;
        glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_STRIP);
        if (filled) glVertex2f(cx, cy);
        for (int i = 0; i <= seg; ++i) {
            float a = start + (M_PI / 2.0f) * (float)i / seg;
            glVertex2f(cx + std::cos(a) * r, cy + std::sin(a) * r);
        }
        glEnd();
    }
}

void drawPanel(float x1, float y1, float x2, float y2, float r, float alpha = 0.88f) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.04f, 0.08f, 0.12f, alpha);
    drawRoundedRect(x1, y1, x2, y2, r, true);
    glColor4f(0.18f, 0.78f, 0.95f, 0.75f);
    glLineWidth(1.5f);
    drawRoundedRect(x1, y1, x2, y2, r, false);
    glDisable(GL_BLEND);
}

void setNotification(const std::string& text, float life = 2.0f, int type = 0) {
    g_notifications.push_back({text, life, life, type});
}

Vec2 getShakeOffset() {
    if (g_cameraShake <= 0.0f) return {0, 0};
    float a = g_cameraShake * 10.0f;
    return {randFloat(-a, a), randFloat(-a, a)};
}

void addCameraShake(float v) { g_cameraShake = std::min(1.0f, g_cameraShake + v); }

void initStars() {
    g_stars.clear();
    for (int i = 0; i < 220; ++i) g_stars.push_back({randFloat(0, (float)SCREEN_WIDTH), randFloat(0, (float)SCREEN_HEIGHT), randFloat(0, 6.28f), randFloat(0.6f, 1.8f)});
}

bool overlapsExisting(const Vec2& p, float minDist) {
    for (const auto& t : g_targets) if (distance2D(p, t.basePos) < minDist) return true;
    return false;
}

void initTargets() {
    g_targets.clear();
    int count = randInt(4, 7);
    for (int i = 0; i < count; ++i) {
        Vec2 p;
        int tries = 0;
        do {
            p = {(float)randInt(820, 1220), (float)randInt(240, 680)};
            tries++;
        } while (overlapsExisting(p, 110.0f) && tries < 100);
        TargetShape shape = (TargetShape)randInt(0, 3);
        float size = (float)randInt(18, 28);
        g_targets.push_back({p, p, size, randFloat(0, 6.28f), false, false, i + 1, shape});
    }
}

void resetMission() {
    g_satellite = SatelliteRecon();
    g_launcher = LauncherVehicle();
    g_missile = BallisticMissile();
    g_explosion = Explosion();
    g_smoke.clear();
    g_trail.clear();
    g_debris.clear();
    g_notifications.clear();
    g_groundShockwaves.clear();
    g_dustWaves.clear();
    initTargets();
    g_appMode = MODE_START_SCREEN;
    g_seqState = SEQ_IDLE;
    g_selectedTarget = -1;
    g_score = 0;
    g_destroyedCount = 0;
    g_missileCount = 0;
    g_screenFlash = 0.0f;
    g_cameraShake = 0.0f;
    g_showMissionPanel = true;
    g_countdownTimer = 0.0f;
    std::stringstream ss; ss << "HE THONG SAN SANG - " << g_targets.size() << " MUC TIEU";
    setNotification(ss.str(), 2.2f, 2);
}

void initSimulation() {
    std::srand((unsigned)std::time(nullptr));
    initStars();
    resetMission();
}

void drawBackground() {
    glBegin(GL_QUADS);
    glColor3f(0.02f, 0.05f, 0.10f); glVertex2f(0, SCREEN_HEIGHT);
    glColor3f(0.02f, 0.05f, 0.10f); glVertex2f(SCREEN_WIDTH, SCREEN_HEIGHT);
    glColor3f(0.00f, 0.01f, 0.04f); glVertex2f(SCREEN_WIDTH, 0);
    glColor3f(0.00f, 0.01f, 0.04f); glVertex2f(0, 0);
    glEnd();
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (const auto& s : g_stars) {
        float a = 0.35f + 0.65f * std::fabs(std::sin(g_uiPulse * s.speed + s.phase));
        glColor4f(a, a, a, 1.0f);
        glVertex2f(s.x, s.y);
    }
    glEnd();
}

void drawGround() {
    glBegin(GL_QUADS);
    glColor3f(0.08f, 0.10f, 0.12f); glVertex2f(0, 0);
    glColor3f(0.10f, 0.12f, 0.14f); glVertex2f(SCREEN_WIDTH, 0);
    glColor3f(0.14f, 0.16f, 0.18f); glVertex2f(SCREEN_WIDTH, 180);
    glColor3f(0.12f, 0.14f, 0.16f); glVertex2f(0, 180);
    glEnd();
}

void drawRadar() {
    Vec2 c(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f + 20.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (g_showGrid) {
        glColor4f(0.0f, 0.9f, 0.2f, 0.08f);
        glBegin(GL_LINES);
        for (int x = 0; x <= SCREEN_WIDTH; x += 50) { glVertex2f((float)x, 0); glVertex2f((float)x, SCREEN_HEIGHT); }
        for (int y = 0; y <= SCREEN_HEIGHT; y += 50) { glVertex2f(0, (float)y); glVertex2f(SCREEN_WIDTH, (float)y); }
        glEnd();
    }
    glColor4f(0.0f, 1.0f, 0.45f, 0.14f);
    for (int r = 100; r <= 520; r += 90) {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 360; i += 4) {
            float a = i * M_PI / 180.0f;
            glVertex2f(c.x + r * std::cos(a), c.y + r * std::sin(a));
        }
        glEnd();
    }
    glColor4f(0.0f, 1.0f, 0.55f, 0.12f);
    glBegin(GL_TRIANGLES);
    glVertex2f(c.x, c.y);
    glVertex2f(c.x + 520 * std::cos(g_radarSweep - 0.12f), c.y + 520 * std::sin(g_radarSweep - 0.12f));
    glVertex2f(c.x + 520 * std::cos(g_radarSweep + 0.12f), c.y + 520 * std::sin(g_radarSweep + 0.12f));
    glEnd();
    glColor4f(0.25f, 1.0f, 0.75f, 0.55f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(c.x, c.y);
    glVertex2f(c.x + 520 * std::cos(g_radarSweep), c.y + 520 * std::sin(g_radarSweep));
    glEnd();
    glDisable(GL_BLEND);
}

void drawSatellite() {
    Vec2 p = g_satellite.position();
    Vec2 sh = getShakeOffset();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.20f, 0.65f, 1.0f, 0.14f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 360; i += 6) {
        float a = i * M_PI / 180.0f;
        glVertex2f(g_satellite.center.x + std::cos(a) * g_satellite.orbitRadius,
                   g_satellite.center.y + std::sin(a) * g_satellite.orbitRadius);
    }
    glEnd();
    if (g_selectedTarget >= 0 && g_selectedTarget < (int)g_targets.size() && !g_targets[g_selectedTarget].destroyed) {
        Vec2 tp = g_targets[g_selectedTarget].pos;
        glLineWidth(2.5f);
        glColor4f(1.0f, 0.15f, 0.12f, 0.50f + 0.20f * std::sin(g_worldTime * 8.0f));
        glBegin(GL_LINES);
        glVertex2f(p.x + sh.x, p.y + sh.y);
        glVertex2f(tp.x, tp.y);
        glEnd();
    }
    glPushMatrix();
    glTranslatef(p.x + sh.x, p.y + sh.y, 0);
    glRotatef(g_satellite.angle * 180.0f / M_PI, 0, 0, 1);
    glColor3f(0.80f, 0.84f, 0.92f);
    glBegin(GL_QUADS);
    glVertex2f(-22, -14); glVertex2f(22, -14); glVertex2f(22, 14); glVertex2f(-22, 14);
    glEnd();
    glColor3f(0.25f, 0.52f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(-56, -8); glVertex2f(-24, -8); glVertex2f(-24, 8); glVertex2f(-56, 8);
    glVertex2f(24, -8); glVertex2f(56, -8); glVertex2f(56, 8); glVertex2f(24, 8);
    glEnd();
    glColor3f(0.98f, 0.78f, 0.20f);
    drawCircle(0, 0, 7, 20, true);
    float pulse = 0.5f + 0.5f * std::sin(g_satellite.pulse * 5.0f);
    glColor3f(1.0f, 0.35f + 0.55f * pulse, 0.12f);
    glPointSize(7.0f);
    glBegin(GL_POINTS);
    glVertex2f(0, 26);
    glEnd();
    glPopMatrix();
    glDisable(GL_BLEND);
}

void drawTargets() {
    for (const auto& target : g_targets) {
        if (target.destroyed) continue;
        glPushMatrix();
        glTranslatef(target.pos.x, target.pos.y, 0);
        float pulse = 0.5f + 0.5f * std::sin(g_worldTime * 5.0f + target.phase);
        if (target.locked) glColor3f(1.0f, 0.92f, 0.15f); else glColor3f(1.0f, 0.20f, 0.18f);
        glLineWidth(2.2f);
        drawPolyShape(target.shape, target.size, false);
        glBegin(GL_LINES);
        glVertex2f(-target.size, 0); glVertex2f(target.size, 0);
        glVertex2f(0, -target.size); glVertex2f(0, target.size);
        glEnd();
        if (target.locked) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1.0f, 0.92f, 0.20f, 0.48f);
            drawCircle(0, 0, target.size + 13 + pulse * 4, 28, false);
            glDisable(GL_BLEND);
        }
        drawText(-10, -target.size - 18, "T" + std::to_string(target.id), GLUT_BITMAP_HELVETICA_12);
        glPopMatrix();
    }
}

void drawLauncher() {
    Vec2 p = g_launcher.pos;
    Vec2 sh = getShakeOffset();
    float alert = 0.5f + 0.5f * std::sin(g_launcher.warningPulse * 6.0f);
    glPushMatrix();
    glTranslatef(p.x + sh.x, p.y + sh.y, 0);
    glColor3f(0.25f, 0.29f, 0.33f);
    glBegin(GL_QUADS);
    glVertex2f(-72, 0); glVertex2f(70, 0); glVertex2f(82, 30); glVertex2f(-60, 30);
    glEnd();
    glColor3f(0.18f, 0.21f, 0.24f);
    glBegin(GL_QUADS);
    glVertex2f(-28, 30); glVertex2f(14, 30); glVertex2f(24, 54); glVertex2f(-18, 54);
    glEnd();
    glColor3f(1.0f, 0.2f + 0.7f * alert, 0.1f);
    drawCircle(-50, 24, 4, 12, true);
    drawCircle(58, 24, 4, 12, true);
    glColor3f(0.08f, 0.08f, 0.08f);
    drawCircle(-40, -3, 12, 18, true);
    drawCircle(0, -3, 12, 18, true);
    drawCircle(40, -3, 12, 18, true);
    glPushMatrix();
    glTranslatef(36, 42, 0);
    glColor3f(0.20f, 0.24f, 0.28f);
    glBegin(GL_QUADS);
    glVertex2f(-12, -14); glVertex2f(20, -14); glVertex2f(20, 14); glVertex2f(-12, 14);
    glEnd();
    glPushMatrix();
    glTranslatef(4, 0, 0);
    glRotatef(-55.0f * g_launcher.hatchOpen, 0, 0, 1);
    glColor3f(0.30f, 0.34f, 0.38f);
    glBegin(GL_QUADS);
    glVertex2f(-4, -16); glVertex2f(8, -16); glVertex2f(8, 0); glVertex2f(-4, 0);
    glEnd();
    glPopMatrix();
    glPushMatrix();
    glTranslatef(4, 0, 0);
    glRotatef(55.0f * g_launcher.hatchOpen, 0, 0, 1);
    glColor3f(0.30f, 0.34f, 0.38f);
    glBegin(GL_QUADS);
    glVertex2f(-4, 0); glVertex2f(8, 0); glVertex2f(8, 16); glVertex2f(-4, 16);
    glEnd();
    glPopMatrix();
    float angle = g_launcher.erectProgress * 86.0f;
    glRotatef(angle, 0, 0, 1);
    glTranslatef(-g_launcher.recoil, 0, 0);
    glColor3f(0.35f, 0.40f, 0.44f);
    glBegin(GL_QUADS);
    glVertex2f(-16, -10); glVertex2f(118, -10); glVertex2f(118, 10); glVertex2f(-16, 10);
    glEnd();
    glColor3f(0.22f, 0.26f, 0.30f);
    glBegin(GL_LINES);
    glVertex2f(0, -7); glVertex2f(110, -7);
    glVertex2f(0, 7); glVertex2f(110, 7);
    glEnd();
    if (g_seqState == SEQ_LAUNCH_PREP || g_seqState == SEQ_COUNTDOWN) {
        glPushMatrix();
        glTranslatef(72, 0, 0);
        glColor3f(0.95f, 0.97f, 1.0f);
        glBegin(GL_TRIANGLES);
        glVertex2f(36, 0); glVertex2f(18, -7); glVertex2f(18, 7);
        glEnd();
        glColor3f(1.0f, 0.58f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2f(-18, -6); glVertex2f(18, -6); glVertex2f(18, 6); glVertex2f(-18, 6);
        glEnd();
        glColor3f(0.88f, 0.25f, 0.10f);
        glBegin(GL_TRIANGLES);
        glVertex2f(-18, 0); glVertex2f(-28, 5); glVertex2f(-28, -5);
        glEnd();
        glPopMatrix();
    }
    glPopMatrix();
    glPopMatrix();
}

void drawMissile() {
    if (!g_missile.active) return;
    Vec2 sh = getShakeOffset();
    glPushMatrix();
    glTranslatef(g_missile.pos.x + sh.x, g_missile.pos.y + sh.y, 0);
    glRotatef(g_missile.angleDeg, 0, 0, 1);
    glColor3f(0.98f, 0.98f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(18, 0); glVertex2f(-8, -7); glVertex2f(-8, 7);
    glEnd();
    glColor3f(1.0f, 0.58f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2f(-16, -5); glVertex2f(12, -5); glVertex2f(12, 5); glVertex2f(-16, 5);
    glEnd();
    glColor3f(0.88f, 0.25f, 0.10f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-16, 0); glVertex2f(-28, 6); glVertex2f(-28, -6);
    glEnd();

    // upgraded booster flame by phase
    float baseFlame = 10.0f;
    if (g_seqState == SEQ_ASCENT) baseFlame = 22.0f;
    else if (g_seqState == SEQ_PITCHOVER) baseFlame = 16.0f;
    else if (g_seqState == SEQ_BALLISTIC) baseFlame = 10.0f;
    float flame = baseFlame + 5.0f * std::sin(g_worldTime * 26.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(1.0f, 0.45f, 0.08f, 0.50f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-24, 0); glVertex2f(-52, -flame * 0.7f); glVertex2f(-52, flame * 0.7f);
    glEnd();
    glColor4f(1.0f, 0.78f, 0.15f, 0.95f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-26, 0); glVertex2f(-42, -flame * 0.45f); glVertex2f(-42, flame * 0.45f);
    glEnd();
    glDisable(GL_BLEND);
    glPopMatrix();
}

void drawSmoke() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto& s : g_smoke) {
        glColor4f(0.60f, 0.60f, 0.62f, s.life * 0.38f);
        drawCircle(s.pos.x, s.pos.y, s.size, 18, true);
    }
    glDisable(GL_BLEND);
}

void drawTrail() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (const auto& t : g_trail) {
        glColor4f(1.0f, 0.56f, 0.10f, t.life);
        drawCircle(t.pos.x, t.pos.y, t.size, 14, true);
    }
    glDisable(GL_BLEND);
}

void drawDebris() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto& d : g_debris) {
        glPushMatrix();
        glTranslatef(d.pos.x, d.pos.y, 0);
        glRotatef(d.rot, 0, 0, 1);
        glColor4f(1.0f, 0.72f, 0.22f, d.life);
        glBegin(GL_QUADS);
        glVertex2f(-d.size, -d.size); glVertex2f(d.size, -d.size);
        glVertex2f(d.size, d.size); glVertex2f(-d.size, d.size);
        glEnd();
        glPopMatrix();
    }
    glDisable(GL_BLEND);
}

void drawGroundEffects() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto& s : g_groundShockwaves) {
        if (!s.active) continue;
        glColor4f(1.0f, 0.82f, 0.26f, s.alpha);
        glLineWidth(3.0f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 40; ++i) {
            float a = M_PI * i / 40.0f;
            glVertex2f(s.pos.x + std::cos(a) * s.radius, 180.0f + std::sin(a) * s.radius * 0.22f);
        }
        glEnd();
    }
    for (const auto& d : g_dustWaves) {
        glColor4f(0.55f, 0.50f, 0.44f, d.life * 0.35f);
        drawCircle(d.pos.x, d.pos.y, d.size, 20, true);
    }
    glDisable(GL_BLEND);
}

void drawExplosion() {
    if (!g_explosion.active) return;
    float t = g_explosion.time / g_explosion.duration;
    float r = g_explosion.radius + 150.0f * t;
    float a = 1.0f - t * 0.8f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(1.0f, 0.85f, 0.35f, a);
    drawCircle(g_explosion.pos.x, g_explosion.pos.y, r * 0.35f, 40, true);
    glColor4f(1.0f, 0.42f, 0.08f, a * 0.75f);
    drawCircle(g_explosion.pos.x, g_explosion.pos.y, r * 0.70f, 40, true);
    glColor4f(1.0f, 0.22f, 0.04f, a * 0.40f);
    drawCircle(g_explosion.pos.x, g_explosion.pos.y, r, 40, true);

    // mushroom cloud
    float cloudAlpha = clampf(1.0f - (t - 0.08f) * 0.9f, 0.0f, 0.7f);
    if (t > 0.05f) {
        float stemH = 90.0f + 180.0f * t;
        float stemW = 18.0f + 20.0f * t;
        glColor4f(0.35f, 0.32f, 0.30f, cloudAlpha * 0.6f);
        drawRoundedRect(g_explosion.pos.x - stemW, g_explosion.pos.y, g_explosion.pos.x + stemW, g_explosion.pos.y + stemH, 10, true);
        float capY = g_explosion.pos.y + stemH;
        glColor4f(0.42f, 0.38f, 0.34f, cloudAlpha);
        drawCircle(g_explosion.pos.x - 40.0f * t, capY + 10.0f, 30.0f + 45.0f * t, 28, true);
        drawCircle(g_explosion.pos.x + 40.0f * t, capY + 14.0f, 28.0f + 42.0f * t, 28, true);
        drawCircle(g_explosion.pos.x, capY + 28.0f, 40.0f + 58.0f * t, 32, true);
    }
    glDisable(GL_BLEND);
}

std::string seqLabel() {
    switch (g_seqState) {
        case SEQ_IDLE: return "STANDBY";
        case SEQ_TARGET_LOCKED: return "TARGET LOCKED";
        case SEQ_COUNTDOWN: return "COUNTDOWN";
        case SEQ_LAUNCH_PREP: return "MISSILE ERECTING";
        case SEQ_ASCENT: return "VERTICAL ASCENT";
        case SEQ_PITCHOVER: return "PITCH OVER";
        case SEQ_BALLISTIC: return "BALLISTIC FLIGHT";
        case SEQ_IMPACT: return "DETONATION";
        default: return "UNKNOWN";
    }
}

void drawTopBar() {
    drawPanel(20, SCREEN_HEIGHT - 52, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 12, 12, 0.86f);
    glColor3f(0.94f, 0.98f, 1.0f);
    drawText(40, SCREEN_HEIGHT - 31, "RECON SATELLITE / GROUND LAUNCH STRIKE", GLUT_BITMAP_HELVETICA_18);
    std::stringstream ss;
    ss << "Score: " << g_score << "  |  Destroyed: " << g_destroyedCount << "/" << g_targets.size() << "  |  Missiles: " << g_missileCount;
    glColor3f(0.70f, 0.93f, 1.0f);
    drawText(SCREEN_WIDTH - 360, SCREEN_HEIGHT - 31, ss.str(), GLUT_BITMAP_HELVETICA_12);
}

void drawLeftPanel() {
    drawPanel(20, 470, 310, SCREEN_HEIGHT - 74, 16, 0.87f);
    glColor3f(0.94f, 0.98f, 1.0f);
    drawText(40, SCREEN_HEIGHT - 104, "TACTICAL STATUS", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.72f, 0.94f, 1.0f);
    drawText(40, SCREEN_HEIGHT - 140, "Sequence:", GLUT_BITMAP_HELVETICA_12);
    glColor3f(1.0f, 0.88f, 0.30f);
    drawText(120, SCREEN_HEIGHT - 140, seqLabel(), GLUT_BITMAP_HELVETICA_12);
    Vec2 sp = g_satellite.position();
    std::stringstream s1, s2;
    s1 << "SAT: (" << (int)sp.x << ", " << (int)sp.y << ")";
    s2 << "LAUNCHER: (" << (int)g_launcher.pos.x << ", " << (int)g_launcher.pos.y << ")";
    glColor3f(0.80f, 0.95f, 1.0f);
    drawText(40, SCREEN_HEIGHT - 172, s1.str(), GLUT_BITMAP_HELVETICA_12);
    drawText(40, SCREEN_HEIGHT - 194, s2.str(), GLUT_BITMAP_HELVETICA_12);
    if (g_selectedTarget >= 0 && g_selectedTarget < (int)g_targets.size() && !g_targets[g_selectedTarget].destroyed) {
        Vec2 tp = g_targets[g_selectedTarget].pos;
        float d = distance2D(g_launcher.pos, tp);
        std::stringstream t1, t2;
        t1 << "Target: T" << g_targets[g_selectedTarget].id;
        t2 << "Distance: " << (int)d << " m";
        glColor3f(1.0f, 0.58f, 0.30f);
        drawText(40, SCREEN_HEIGHT - 236, t1.str(), GLUT_BITMAP_HELVETICA_18);
        glColor3f(0.82f, 0.96f, 1.0f);
        drawText(40, SCREEN_HEIGHT - 260, t2.str(), GLUT_BITMAP_HELVETICA_12);
    } else {
        glColor3f(0.80f, 0.94f, 1.0f);
        drawText(40, SCREEN_HEIGHT - 236, "No target locked", GLUT_BITMAP_HELVETICA_18);
    }
    glColor3f(0.92f, 0.98f, 1.0f);
    drawText(40, SCREEN_HEIGHT - 320, "Launcher elevation", GLUT_BITMAP_HELVETICA_12);
    glColor3f(0.20f, 0.85f, 0.35f);
    drawFilledRect(40, SCREEN_HEIGHT - 346, 40 + 220 * g_launcher.erectProgress, SCREEN_HEIGHT - 360);
    glColor3f(0.18f, 0.78f, 0.95f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(40, SCREEN_HEIGHT - 346); glVertex2f(260, SCREEN_HEIGHT - 346); glVertex2f(260, SCREEN_HEIGHT - 360); glVertex2f(40, SCREEN_HEIGHT - 360);
    glEnd();
}

void drawMissionPanel() {
    float x1 = SCREEN_WIDTH - 280, y1 = 500, x2 = SCREEN_WIDTH - 20, y2 = SCREEN_HEIGHT - 74;
    drawPanel(x1, y1, x2, y2, 16, 0.87f);
    glColor3f(0.94f, 0.98f, 1.0f);
    drawText(x1 + 18, y2 - 30, "MISSION CONTROL", GLUT_BITMAP_HELVETICA_18);
    float bx1 = x2 - 42, by1 = y2 - 38, bx2 = x2 - 14, by2 = y2 - 12;
    bool hoverClose = pointInRect(g_mousePos.x, g_mousePos.y, bx1, by1, bx2, by2);
    if (hoverClose) glColor4f(0.95f, 0.25f, 0.25f, 0.95f); else glColor4f(0.55f, 0.18f, 0.18f, 0.85f);
    drawRoundedRect(bx1, by1, bx2, by2, 6, true);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawTextCentered((bx1 + bx2) / 2.0f, by1 + 7, "X", GLUT_BITMAP_HELVETICA_12);
    bool canAttack = (g_appMode == MODE_COMMAND_SCREEN && g_seqState == SEQ_TARGET_LOCKED);
    float btnX1 = x1 + 20, btnY1 = y2 - 92, btnX2 = x2 - 20, btnY2 = y2 - 52;
    bool hoverLaunch = pointInRect(g_mousePos.x, g_mousePos.y, btnX1, btnY1, btnX2, btnY2);
    if (canAttack) { if (hoverLaunch) glColor4f(0.28f, 0.90f, 0.30f, 0.92f); else glColor4f(0.20f, 0.65f, 0.22f, 0.88f); }
    else glColor4f(0.16f, 0.30f, 0.18f, 0.60f);
    drawRoundedRect(btnX1, btnY1, btnX2, btnY2, 10, true);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawTextCentered((btnX1 + btnX2) / 2.0f, btnY1 + 14, "LAUNCH ATTACK", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.76f, 0.94f, 1.0f);
    drawText(x1 + 20, y2 - 130, "Upgrade Pack 2: booster + shockwave", GLUT_BITMAP_HELVETICA_12);
    drawText(x1 + 20, y2 - 150, "Reset se doi so luong + hinh target", GLUT_BITMAP_HELVETICA_12);
    std::stringstream s1, s2;
    s1 << "Targets: " << g_targets.size();
    s2 << "Accuracy: " << (g_missileCount > 0 ? (g_destroyedCount * 100 / g_missileCount) : 0) << "%";
    drawText(x1 + 20, y2 - 185, s1.str(), GLUT_BITMAP_HELVETICA_12);
    drawText(x1 + 20, y2 - 207, s2.str(), GLUT_BITMAP_HELVETICA_12);
}

void drawCountdownOverlay() {
    if (g_seqState != SEQ_COUNTDOWN) return;
    int shown = std::max(1, 3 - (int)g_countdownTimer);
    float pulse = 0.6f + 0.4f * std::fabs(std::sin(g_worldTime * 8.0f));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.12f);
    drawFilledRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glColor4f(1.0f, 0.30f + 0.50f * pulse, 0.12f, 0.85f);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f + 30, "T-" + std::to_string(shown), GLUT_BITMAP_TIMES_ROMAN_24);
    glColor4f(1.0f, 0.92f, 0.80f, 0.92f);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f - 10, "LAUNCH COUNTDOWN", GLUT_BITMAP_HELVETICA_18);
    glDisable(GL_BLEND);
}

void drawMinimap() {
    if (!g_showMinimap) return;
    float x = SCREEN_WIDTH - 205, y = 115, size = 170;
    drawPanel(x - 10, y - 10, x + size + 10, y + size + 10, 12, 0.82f);
    glColor4f(0.0f, 0.12f, 0.22f, 0.35f);
    drawFilledRect(x, y, x + size, y + size);
    glColor4f(0.0f, 0.85f, 0.35f, 0.14f);
    glBegin(GL_LINES);
    for (int i = 0; i <= 4; ++i) {
        float px = x + i * size / 4.0f;
        float py = y + i * size / 4.0f;
        glVertex2f(px, y); glVertex2f(px, y + size);
        glVertex2f(x, py); glVertex2f(x + size, py);
    }
    glEnd();
    Vec2 sp = g_satellite.position();
    float sx = x + (sp.x / SCREEN_WIDTH) * size;
    float sy = y + (sp.y / SCREEN_HEIGHT) * size;
    glColor3f(0.30f, 0.80f, 1.0f); drawCircle(sx, sy, 4, 10, true);
    for (const auto& t : g_targets) {
        if (t.destroyed) continue;
        float tx = x + (t.pos.x / SCREEN_WIDTH) * size;
        float ty = y + (t.pos.y / SCREEN_HEIGHT) * size;
        if (t.locked) glColor3f(1.0f, 0.92f, 0.18f); else glColor3f(1.0f, 0.25f, 0.22f);
        drawCircle(tx, ty, 4, 10, true);
    }
    if (g_missile.active) {
        float mx = x + (g_missile.pos.x / SCREEN_WIDTH) * size;
        float my = y + (g_missile.pos.y / SCREEN_HEIGHT) * size;
        glColor3f(1.0f, 0.55f, 0.10f); drawCircle(mx, my, 3, 8, true);
    }
}

void drawBottomBar() {
    drawPanel(20, 18, SCREEN_WIDTH - 20, 78, 12, 0.86f);
    glColor3f(0.94f, 0.98f, 1.0f);
    drawText(40, 53, "CONTROLS", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.76f, 0.94f, 1.0f);
    if (g_appMode == MODE_START_SCREEN)
        drawText(40, 31, "ENTER / SPACE: Start   |   ESC: Exit", GLUT_BITMAP_HELVETICA_12);
    else
        drawText(40, 31, "Click: Select target   |   SPACE: Launch   |   H: Show panel   |   T: Minimap   |   G: Grid   |   R: Reset   |   ESC: Menu", GLUT_BITMAP_HELVETICA_12);
}

void drawNotifications() {
    float y = SCREEN_HEIGHT - 110;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto& n : g_notifications) {
        float a = clampf(n.timer / n.life, 0.0f, 1.0f);
        float r = 0.18f, g = 0.60f, b = 1.0f;
        if (n.type == 1) { r = 1.0f; g = 0.82f; b = 0.25f; }
        if (n.type == 2) { r = 0.25f; g = 1.0f; b = 0.45f; }
        glColor4f(0.04f, 0.08f, 0.14f, 0.82f * a);
        drawRoundedRect(SCREEN_WIDTH - 390, y - 10, SCREEN_WIDTH - 22, y + 20, 8, true);
        glColor4f(r, g, b, a);
        drawRoundedRect(SCREEN_WIDTH - 390, y - 10, SCREEN_WIDTH - 22, y + 20, 8, false);
        glColor4f(1.0f, 1.0f, 1.0f, a);
        drawText(SCREEN_WIDTH - 376, y, n.text, GLUT_BITMAP_HELVETICA_12);
        y -= 36;
    }
    glDisable(GL_BLEND);
}

void drawFPS() {
    g_frameCount++;
    float now = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    if (now - g_fpsTimer > 0.5f) {
        g_fps = g_frameCount / (now - g_fpsTimer);
        g_frameCount = 0;
        g_fpsTimer = now;
    }
    std::stringstream ss; ss << "FPS: " << std::fixed << std::setprecision(1) << g_fps;
    glColor3f(0.64f, 0.92f, 1.0f);
    drawText(SCREEN_WIDTH - 96, SCREEN_HEIGHT - 30, ss.str(), GLUT_BITMAP_HELVETICA_12);
}

void drawStartScreen() {
    drawBackground();
    drawPanel(250, 205, SCREEN_WIDTH - 250, SCREEN_HEIGHT - 205, 28, 0.90f);
    glColor3f(0.94f, 0.98f, 1.0f);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 300, "RECON SATELLITE & GROUND LAUNCHER", GLUT_BITMAP_TIMES_ROMAN_24);
    glColor3f(0.72f, 0.94f, 1.0f);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 340, "UPGRADE PACK 2 - BOOSTER, SHOCKWAVE, RANDOM TARGETS", GLUT_BITMAP_HELVETICA_18);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 420, "Reset se doi so muc tieu va hinh dang de ban mo phong khong bi lap", GLUT_BITMAP_HELVETICA_12);
    float bx1 = SCREEN_WIDTH / 2.0f - 160, by1 = SCREEN_HEIGHT / 2.0f - 52, bx2 = SCREEN_WIDTH / 2.0f + 160, by2 = SCREEN_HEIGHT / 2.0f;
    bool hover = pointInRect(g_mousePos.x, g_mousePos.y, bx1, by1, bx2, by2);
    if (hover) glColor4f(0.28f, 0.90f, 0.30f, 0.94f); else glColor4f(0.20f, 0.62f, 0.22f, 0.88f);
    drawRoundedRect(bx1, by1, bx2, by2, 10, true);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawTextCentered(SCREEN_WIDTH / 2.0f, by1 + 16, "BAT DAU MO PHONG", GLUT_BITMAP_HELVETICA_18);
}

void drawMissionComplete() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.68f);
    drawFilledRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    drawPanel(430, 300, SCREEN_WIDTH - 430, SCREEN_HEIGHT - 300, 22, 0.92f);
    glColor3f(1.0f, 0.90f, 0.32f);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 390, "MISSION COMPLETE", GLUT_BITMAP_TIMES_ROMAN_24);
    std::stringstream s1, s2;
    s1 << "Destroyed: " << g_destroyedCount << "/" << g_targets.size();
    s2 << "Final score: " << g_score;
    glColor3f(0.92f, 0.98f, 1.0f);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 455, s1.str(), GLUT_BITMAP_HELVETICA_18);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 490, s2.str(), GLUT_BITMAP_HELVETICA_18);
    drawTextCentered(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 560, "Press R to restart", GLUT_BITMAP_HELVETICA_12);
    glDisable(GL_BLEND);
}

void drawScreenFlash() {
    if (g_screenFlash <= 0.0f) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.90f, 0.60f, g_screenFlash);
    drawFilledRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glDisable(GL_BLEND);
}

void beginAttackSequence() {
    if (g_selectedTarget < 0 || g_selectedTarget >= (int)g_targets.size()) return;
    if (g_targets[g_selectedTarget].destroyed) return;
    g_appMode = MODE_SIM_RUNNING;
    g_seqState = SEQ_COUNTDOWN;
    g_countdownTimer = 0.0f;
    g_launchTimer = 0.0f;
    g_launcher.erectProgress = 0.0f;
    g_launcher.hatchOpen = 0.0f;
    setNotification("COUNTDOWN STARTED", 1.8f, 1);
}

void launchMissile() {
    if (g_selectedTarget < 0) return;
    Vec2 start = g_launcher.missileBasePos() + Vec2(0, 14);
    Vec2 target = g_targets[g_selectedTarget].pos;
    g_missile.active = true;
    g_missile.startPos = start;
    g_missile.pos = start;
    g_missile.targetPos = target;
    g_missile.age = 0.0f;
    g_missile.vel = {0.0f, 260.0f};
    g_missile.angleDeg = 90.0f;
    g_missile.booster = 1.0f;
    g_seqState = SEQ_ASCENT;
    g_missileCount++;
    g_launcher.recoil = 14.0f;
    g_screenFlash = 0.18f;
    addCameraShake(0.22f);
    setNotification("MISSILE LAUNCHED", 2.0f, 0);
    for (int i = 0; i < 34; ++i) g_smoke.push_back({start, {randFloat(-28, 28), randFloat(30, 100)}, randFloat(0.8f, 1.3f), randFloat(6, 14)});
}

void triggerExplosion(const Vec2& pos) {
    g_explosion.active = true;
    g_explosion.pos = pos;
    g_explosion.time = 0.0f;
    g_explosion.radius = 34.0f;
    g_seqState = SEQ_IMPACT;
    g_screenFlash = 0.75f;
    addCameraShake(0.95f);
    setNotification("TARGET DESTROYED", 2.2f, 2);
    g_groundShockwaves.push_back({pos, 20.0f, 0.9f, true});
    for (int i = 0; i < 20; ++i) {
        float dir = (i % 2 == 0) ? 1.0f : -1.0f;
        g_dustWaves.push_back({{pos.x, 178.0f}, {dir * randFloat(55, 140), randFloat(8, 28)}, randFloat(0.9f, 1.5f), randFloat(10, 20)});
    }
    for (int i = 0; i < 58; ++i) {
        float a = randFloat(0, 2 * M_PI);
        float sp = randFloat(70, 230);
        g_debris.push_back({pos, {std::cos(a) * sp, std::sin(a) * sp}, randFloat(0.9f, 1.8f), randFloat(2.0f, 5.0f), randFloat(0, 360), randFloat(-220, 220)});
    }
    for (int i = 0; i < 58; ++i) g_smoke.push_back({pos, {randFloat(-40, 40), randFloat(15, 120)}, randFloat(1.3f, 2.2f), randFloat(10, 24)});
}

void updateParticles(float dt) {
    for (auto& s : g_smoke) {
        s.pos = s.pos + s.vel * dt;
        s.vel.y += 16.0f * dt;
        s.life -= dt * 0.60f;
        s.size += dt * 4.8f;
    }
    g_smoke.erase(std::remove_if(g_smoke.begin(), g_smoke.end(), [](const Smoke& s){ return s.life <= 0.0f; }), g_smoke.end());
    for (auto& d : g_debris) {
        d.pos = d.pos + d.vel * dt;
        d.vel.y -= 170.0f * dt;
        d.life -= dt;
        d.rot += d.rotSpeed * dt;
    }
    g_debris.erase(std::remove_if(g_debris.begin(), g_debris.end(), [](const Debris& d){ return d.life <= 0.0f; }), g_debris.end());
    for (auto& t : g_trail) t.life -= dt * 1.9f;
    g_trail.erase(std::remove_if(g_trail.begin(), g_trail.end(), [](const Trail& t){ return t.life <= 0.0f; }), g_trail.end());
    for (auto& n : g_notifications) n.timer -= dt;
    g_notifications.erase(std::remove_if(g_notifications.begin(), g_notifications.end(), [](const Notification& n){ return n.timer <= 0.0f; }), g_notifications.end());
    for (auto& s : g_groundShockwaves) {
        s.radius += 240.0f * dt;
        s.alpha -= dt * 0.55f;
        s.active = s.alpha > 0.0f;
    }
    g_groundShockwaves.erase(std::remove_if(g_groundShockwaves.begin(), g_groundShockwaves.end(), [](const GroundShockwave& s){ return !s.active; }), g_groundShockwaves.end());
    for (auto& d : g_dustWaves) {
        d.pos = d.pos + d.vel * dt;
        d.life -= dt * 0.8f;
        d.size += dt * 10.0f;
        d.vel.x *= 0.985f;
    }
    g_dustWaves.erase(std::remove_if(g_dustWaves.begin(), g_dustWaves.end(), [](const DustWave& d){ return d.life <= 0.0f; }), g_dustWaves.end());
    g_cameraShake = std::max(0.0f, g_cameraShake - dt * 2.1f);
    g_launcher.recoil = std::max(0.0f, g_launcher.recoil - dt * 18.0f);
}

void updateSimulation(float dt) {
    g_worldTime += dt;
    g_uiPulse += dt;
    g_radarSweep += dt * 1.8f;
    if (g_radarSweep > 2 * M_PI) g_radarSweep -= 2 * M_PI;
    g_screenFlash = std::max(0.0f, g_screenFlash - dt * 0.85f);
    g_launcher.warningPulse += dt;
    for (auto& t : g_targets) t.update(g_worldTime);
    g_satellite.update(dt);
    updateParticles(dt);

    if (g_seqState == SEQ_COUNTDOWN) {
        g_countdownTimer += dt;
        g_launcher.hatchOpen = clampf(g_countdownTimer / 1.2f, 0.0f, 1.0f);
        if (((int)(g_countdownTimer * 10)) % 2 == 0) {
            Vec2 base = g_launcher.missileBasePos();
            g_smoke.push_back({base, {randFloat(-8, 8), randFloat(10, 28)}, 0.55f, randFloat(3, 6)});
        }
        if (g_countdownTimer >= 3.0f) {
            g_seqState = SEQ_LAUNCH_PREP;
            g_launchTimer = 0.0f;
            setNotification("ERECTING MISSILE", 1.2f, 1);
        }
    }

    if (g_seqState == SEQ_LAUNCH_PREP) {
        g_launchTimer += dt;
        g_launcher.erectProgress = clampf(g_launchTimer / 1.1f, 0.0f, 1.0f);
        if (((int)(g_launchTimer * 20)) % 2 == 0) {
            Vec2 base = g_launcher.missileBasePos();
            g_smoke.push_back({base, {randFloat(-10, 10), randFloat(18, 42)}, 0.7f, randFloat(4, 8)});
        }
        if (g_launchTimer >= 1.15f) launchMissile();
    }

    if (g_missile.active) {
        g_missile.age += dt;
        if (g_seqState == SEQ_ASCENT && g_missile.age > 0.55f) g_seqState = SEQ_PITCHOVER;
        if (g_seqState == SEQ_PITCHOVER) {
            Vec2 target = g_missile.targetPos;
            Vec2 pos = g_missile.pos;
            float dx = target.x - pos.x;
            float dy = target.y - pos.y;
            float tFlight = clampf(std::fabs(dx) / 360.0f, 1.4f, 2.8f);
            g_missile.vel.x = dx / tFlight;
            g_missile.vel.y = (dy + 0.5f * g_missile.gravity * tFlight * tFlight) / tFlight;
            g_seqState = SEQ_BALLISTIC;
            g_missile.booster = 0.75f;
        }
        g_missile.vel.y -= g_missile.gravity * dt;
        g_missile.pos = g_missile.pos + g_missile.vel * dt;
        g_missile.angleDeg = std::atan2(g_missile.vel.y, g_missile.vel.x) * 180.0f / M_PI;
        g_trail.push_back({g_missile.pos, 1.0f, randFloat(4, 7)});
        // phase-dependent smoke density
        int smokeBurst = (g_seqState == SEQ_ASCENT ? 4 : (g_seqState == SEQ_PITCHOVER ? 3 : 2));
        for (int i = 0; i < smokeBurst; ++i) g_smoke.push_back({g_missile.pos, {randFloat(-12, 12), randFloat(-5, 26)}, randFloat(0.35f, 0.75f), randFloat(3, 6)});
        if (g_selectedTarget >= 0 && !g_targets[g_selectedTarget].destroyed) {
            float dist = distance2D(g_missile.pos, g_targets[g_selectedTarget].pos);
            if (dist < 28.0f) {
                g_targets[g_selectedTarget].destroyed = true;
                g_targets[g_selectedTarget].locked = false;
                g_missile.active = false;
                triggerExplosion(g_targets[g_selectedTarget].pos);
                g_score += 10;
                g_destroyedCount++;
                g_selectedTarget = -1;
            }
        }
        if (g_missile.pos.x < -50 || g_missile.pos.x > SCREEN_WIDTH + 50 || g_missile.pos.y > SCREEN_HEIGHT + 50 || g_missile.pos.y < -50) {
            g_missile.active = false;
            g_seqState = SEQ_IDLE;
            g_appMode = MODE_COMMAND_SCREEN;
        }
    }

    if (g_explosion.active) {
        g_explosion.time += dt;
        g_explosion.radius += dt * 120.0f;
        if (g_explosion.time >= g_explosion.duration) {
            g_explosion.active = false;
            g_seqState = SEQ_IDLE;
            bool allDestroyed = true;
            for (const auto& t : g_targets) if (!t.destroyed) { allDestroyed = false; break; }
            if (allDestroyed) g_appMode = MODE_MISSION_COMPLETE;
            else g_appMode = MODE_COMMAND_SCREEN;
        }
    }
}

void display() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (g_appMode == MODE_START_SCREEN) {
        drawStartScreen();
        drawBottomBar();
        drawFPS();
        drawScreenFlash();
        glutSwapBuffers();
        return;
    }
    drawBackground();
    drawRadar();
    drawGround();
    drawGroundEffects();
    drawSatellite();
    drawTargets();
    drawLauncher();
    drawMissile();
    drawSmoke();
    drawTrail();
    drawExplosion();
    drawDebris();
    if (g_showMinimap) drawMinimap();
    drawTopBar();
    drawLeftPanel();
    if (g_showMissionPanel) drawMissionPanel();
    drawBottomBar();
    drawNotifications();
    drawFPS();
    drawCountdownOverlay();
    if (g_appMode == MODE_MISSION_COMPLETE) drawMissionComplete();
    drawScreenFlash();
    glutSwapBuffers();
}

void clearLocks() { for (auto& t : g_targets) t.locked = false; }

void mouse(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    y = SCREEN_HEIGHT - y;
    g_mousePos = {(float)x, (float)y};
    if (g_appMode == MODE_START_SCREEN) {
        float bx1 = SCREEN_WIDTH / 2.0f - 160, by1 = SCREEN_HEIGHT / 2.0f - 52, bx2 = SCREEN_WIDTH / 2.0f + 160, by2 = SCREEN_HEIGHT / 2.0f;
        if (pointInRect((float)x, (float)y, bx1, by1, bx2, by2)) {
            g_appMode = MODE_COMMAND_SCREEN;
            setNotification("VE TINH TRINH SAT ONLINE", 2.0f, 0);
        }
        glutPostRedisplay();
        return;
    }
    if (g_showMissionPanel) {
        float x1 = SCREEN_WIDTH - 280, y2 = SCREEN_HEIGHT - 74, x2 = SCREEN_WIDTH - 20;
        float bx1 = x2 - 42, by1 = y2 - 38, bx2 = x2 - 14, by2 = y2 - 12;
        if (pointInRect((float)x, (float)y, bx1, by1, bx2, by2)) { g_showMissionPanel = false; glutPostRedisplay(); return; }
        float btnX1 = x1 + 20, btnY1 = y2 - 92, btnX2 = x2 - 20, btnY2 = y2 - 52;
        if (pointInRect((float)x, (float)y, btnX1, btnY1, btnX2, btnY2) && g_appMode == MODE_COMMAND_SCREEN && g_seqState == SEQ_TARGET_LOCKED) { beginAttackSequence(); glutPostRedisplay(); return; }
    }
    if (g_appMode == MODE_COMMAND_SCREEN) {
        float minDist = 60.0f; int closest = -1;
        for (size_t i = 0; i < g_targets.size(); ++i) {
            if (g_targets[i].destroyed) continue;
            float d = distance2D(g_targets[i].pos, {(float)x, (float)y});
            if (d < minDist) { minDist = d; closest = (int)i; }
        }
        if (closest >= 0) {
            clearLocks();
            g_selectedTarget = closest;
            g_targets[g_selectedTarget].locked = true;
            g_seqState = SEQ_TARGET_LOCKED;
            setNotification("TARGET LOCKED", 1.8f, 1);
        }
    }
    glutPostRedisplay();
}

void passiveMotion(int x, int y) {
    y = SCREEN_HEIGHT - y;
    g_mousePos = {(float)x, (float)y};
    glutPostRedisplay();
}

void keyboard(unsigned char key, int, int) {
    if (g_appMode == MODE_START_SCREEN) {
        if (key == 13 || key == ' ') { g_appMode = MODE_COMMAND_SCREEN; setNotification("VE TINH TRINH SAT ONLINE", 2.0f, 0); }
        if (key == 27) std::exit(0);
        return;
    }
    switch (key) {
        case ' ': if (g_appMode == MODE_COMMAND_SCREEN && g_seqState == SEQ_TARGET_LOCKED) beginAttackSequence(); break;
        case 'r': case 'R': resetMission(); g_appMode = MODE_COMMAND_SCREEN; break;
        case 'g': case 'G': g_showGrid = !g_showGrid; break;
        case 't': case 'T': g_showMinimap = !g_showMinimap; break;
        case 'h': case 'H': g_showMissionPanel = !g_showMissionPanel; break;
        case 27: g_appMode = MODE_START_SCREEN; resetMission(); break;
    }
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void update(int) {
    float now = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float dt = now - g_lastTime;
    g_lastTime = now;
    if (dt > 0.1f) dt = 0.1f;
    if (g_appMode != MODE_START_SCREEN) updateSimulation(dt);
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    glutInitWindowPosition(30, 20);
    glutCreateWindow(WINDOW_TITLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(passiveMotion);
    initSimulation();
    g_lastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    glutTimerFunc(0, update, 0);
    glutMainLoop();
    return 0;
}
