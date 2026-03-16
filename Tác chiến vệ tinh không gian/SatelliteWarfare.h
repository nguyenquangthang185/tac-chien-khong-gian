#ifndef SATELLITE_WARFARE_H
#define SATELLITE_WARFARE_H

#include <GL/glut.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>

// Cấu hình màn hình
const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 800;
const char* WINDOW_TITLE = "Hệ thống tác chiến vệ tinh - Nhóm 12";

// Cấu hình màu sắc
const float COLOR_BLACK[] = {0.0f, 0.0f, 0.0f};
const float COLOR_WHITE[] = {1.0f, 1.0f, 1.0f};
const float COLOR_GREEN[] = {0.0f, 1.0f, 0.0f};
const float COLOR_RED[] = {1.0f, 0.0f, 0.0f};
const float COLOR_BLUE[] = {0.0f, 0.0f, 1.0f};
const float COLOR_YELLOW[] = {1.0f, 1.0f, 0.0f};
const float COLOR_ORANGE[] = {1.0f, 0.5f, 0.0f};
const float COLOR_GRAY[] = {0.5f, 0.5f, 0.5f};

// Các trạng thái hệ thống
enum GameState {
    STATE_IDLE,           // Chờ chọn mục tiêu
    STATE_TARGET_LOCKED,  // Đã khóa mục tiêu
    STATE_MISSILE_LAUNCHED, // Đã phóng tên lửa
    STATE_EXPLOSION       // Đang nổ
};

// Lớp Vector2D hỗ trợ tính toán
class Vector2D {
public:
    float x, y;
    
    Vector2D(float x = 0, float y = 0) : x(x), y(y) {}
    
    Vector2D operator+(const Vector2D& v) const { return Vector2D(x + v.x, y + v.y); }
    Vector2D operator-(const Vector2D& v) const { return Vector2D(x - v.x, y - v.y); }
    Vector2D operator*(float s) const { return Vector2D(x * s, y * s); }
    
    float length() const { return sqrt(x*x + y*y); }
    void normalize() { 
        float len = length();
        if (len > 0) { x /= len; y /= len; }
    }
    
    static float distance(const Vector2D& a, const Vector2D& b) {
        return (a - b).length();
    }
};

// Lớp đối tượng cơ sở
class GameObject {
protected:
    Vector2D position;
    Vector2D velocity;
    float size;
    bool active;
    
public:
    GameObject(float x = 0, float y = 0, float s = 10) 
        : position(x, y), velocity(0, 0), size(s), active(true) {}
    
    virtual ~GameObject() {}
    
    virtual void update(float deltaTime) {
        if (active) {
            position = position + velocity * deltaTime;
        }
    }
    
    virtual void render() = 0;
    
    // Getters/Setters
    Vector2D getPosition() const { return position; }
    void setPosition(float x, float y) { position.x = x; position.y = y; }
    float getSize() const { return size; }
    bool isActive() const { return active; }
    void setActive(bool a) { active = a; }
    void setVelocity(float vx, float vy) { velocity.x = vx; velocity.y = vy; }
};

// Lớp vệ tinh
class Satellite : public GameObject {
private:
    float angle;
    float orbitRadius;
    float orbitSpeed;
    Vector2D center;
    
public:
    Satellite(float cx, float cy, float r = 150, float speed = 0.5f) 
        : GameObject(cx + r, cy, 20), center(cx, cy), orbitRadius(r), orbitSpeed(speed), angle(0) {}
    
    void update(float deltaTime) override {
        angle += orbitSpeed * deltaTime;
        if (angle > 2 * M_PI) angle -= 2 * M_PI;
        
        // Chuyển động theo quỹ đạo tròn
        position.x = center.x + orbitRadius * cos(angle);
        position.y = center.y + orbitRadius * sin(angle);
    }
    
    void render() override {
        if (!active) return;
        
        glPushMatrix();
        glTranslatef(position.x, position.y, 0);
        
        // Vẽ thân vệ tinh
        glColor3fv(COLOR_GRAY);
        glBegin(GL_QUADS);
        glVertex2f(-15, -10);
        glVertex2f(15, -10);
        glVertex2f(15, 10);
        glVertex2f(-15, 10);
        glEnd();
        
        // Vẽ cánh năng lượng mặt trời
        glColor3fv(COLOR_BLUE);
        glBegin(GL_QUADS);
        glVertex2f(-25, -5);
        glVertex2f(-15, -5);
        glVertex2f(-15, 5);
        glVertex2f(-25, 5);
        
        glVertex2f(15, -5);
        glVertex2f(25, -5);
        glVertex2f(25, 5);
        glVertex2f(15, 5);
        glEnd();
        
        // Vẽ ăng-ten
        glColor3fv(COLOR_RED);
        glLineWidth(2);
        glBegin(GL_LINES);
        glVertex2f(0, 10);
        glVertex2f(0, 25);
        glEnd();
        
        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex2f(0, 27);
        glEnd();
        
        glPopMatrix();
    }
};

// Lớp mục tiêu
class Target : public GameObject {
private:
    bool locked;
    bool destroyed;
    float pulsePhase;
    
public:
    Target(float x, float y) 
        : GameObject(x, y, 15), locked(false), destroyed(false), pulsePhase(0) {}
    
    void update(float deltaTime) override {
        pulsePhase += deltaTime * 5;
        if (pulsePhase > 2 * M_PI) pulsePhase -= 2 * M_PI;
    }
    
    void render() override {
        if (!active || destroyed) return;
        
        glPushMatrix();
        glTranslatef(position.x, position.y, 0);
        
        // Hiệu ứng nhấp nháy khi bị khóa
        if (locked) {
            float pulse = 0.3f + 0.7f * fabs(sin(pulsePhase));
            glColor3f(1.0f, pulse, 0.0f);
        } else {
            glColor3fv(COLOR_RED);
        }
        
        // Vẽ mục tiêu (hình vuông với chéo)
        glBegin(GL_LINE_LOOP);
        glVertex2f(-size, -size);
        glVertex2f(size, -size);
        glVertex2f(size, size);
        glVertex2f(-size, size);
        glEnd();
        
        glBegin(GL_LINES);
        glVertex2f(-size, -size);
        glVertex2f(size, size);
        glVertex2f(-size, size);
        glVertex2f(size, -size);
        glEnd();
        
        // Vẽ vòng tròn bao quanh khi khóa
        if (locked) {
            glColor3fv(COLOR_YELLOW);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 20; i++) {
                float theta = 2.0f * M_PI * i / 20;
                float r = size + 10 + 5 * sin(pulsePhase * 2);
                glVertex2f(r * cos(theta), r * sin(theta));
            }
            glEnd();
        }
        
        glPopMatrix();
    }
    
    void setLocked(bool l) { locked = l; }
    bool isLocked() const { return locked; }
    void destroy() { destroyed = true; active = false; }
    bool isDestroyed() const { return destroyed; }
};

// Lớp tên lửa
class Missile : public GameObject {
private:
    Vector2D target;
    Vector2D direction;
    float speed;
    bool hasHit;
    float trailLength;
    std::vector<Vector2D> trail;
    
public:
    Missile(float x, float y, float tx, float ty) 
        : GameObject(x, y, 8), target(tx, ty), speed(300), hasHit(false), trailLength(20) {
        
        // Tính hướng bay
        direction = target - position;
        direction.normalize();
        velocity = direction * speed;
        
        // Thêm vị trí ban đầu vào vệt khói
        trail.push_back(position);
    }
    
    void update(float deltaTime) override {
        if (!active || hasHit) return;
        
        // Lưu vệt khói
        trail.push_back(position);
        if (trail.size() > 20) {
            trail.erase(trail.begin());
        }
        
        // Cập nhật vị trí
        position = position + velocity * deltaTime;
        
        // Kiểm tra đến gần mục tiêu
        float dist = Vector2D::distance(position, target);
        if (dist < 30) {
            hasHit = true;
        }
    }
    
    void render() override {
        if (!active) return;
        
        // Vẽ vệt khói
        glColor4f(0.5f, 0.5f, 0.5f, 0.3f);
        glBegin(GL_QUAD_STRIP);
        for (size_t i = 0; i < trail.size(); i++) {
            float alpha = (float)i / trail.size();
            float width = 3 * (1 - alpha);
            
            Vector2D dir;
            if (i < trail.size() - 1) {
                dir = trail[i+1] - trail[i];
            } else {
                dir = velocity * 0.1f;
            }
            dir.normalize();
            
            Vector2D perp(-dir.y, dir.x);
            
            glColor4f(0.5f, 0.5f, 0.5f, alpha * 0.5f);
            glVertex2f(trail[i].x + perp.x * width, trail[i].y + perp.y * width);
            glVertex2f(trail[i].x - perp.x * width, trail[i].y - perp.y * width);
        }
        glEnd();
        
        // Vẽ thân tên lửa
        glPushMatrix();
        glTranslatef(position.x, position.y, 0);
        
        // Xoay theo hướng bay
        float angle = atan2(velocity.y, velocity.x) * 180 / M_PI;
        glRotatef(angle, 0, 0, 1);
        
        glColor3fv(COLOR_WHITE);
        glBegin(GL_TRIANGLES);
        glVertex2f(15, 0);
        glVertex2f(-10, -5);
        glVertex2f(-10, 5);
        glEnd();
        
        glColor3fv(COLOR_ORANGE);
        glBegin(GL_QUADS);
        glVertex2f(-15, -3);
        glVertex2f(-10, -3);
        glVertex2f(-10, 3);
        glVertex2f(-15, 3);
        glEnd();
        
        glPopMatrix();
    }
    
    bool hasHitTarget() const { return hasHit; }
};

// Lớp hiệu ứng nổ
class Explosion : public GameObject {
private:
    float timer;
    float maxTime;
    float radius;
    std::vector<Vector2D> particles;
    
public:
    Explosion(float x, float y) 
        : GameObject(x, y, 0), timer(0), maxTime(1.0f), radius(5) {
        
        // Tạo các hạt cho hiệu ứng nổ
        for (int i = 0; i < 20; i++) {
            float angle = (rand() % 360) * M_PI / 180;
            float speed = 50 + rand() % 100;
            particles.push_back(Vector2D(cos(angle) * speed, sin(angle) * speed));
        }
    }
    
    void update(float deltaTime) override {
        timer += deltaTime;
        
        // Cập nhật các hạt
        for (auto& p : particles) {
            p = p + p * deltaTime * 2;
        }
        
        radius += deltaTime * 50;
        if (timer >= maxTime) {
            active = false;
        }
    }
    
    void render() override {
        float alpha = 1.0f - (timer / maxTime);
        
        // Vẽ vụ nổ chính
        glColor4f(1.0f, 0.5f + 0.5f * sin(timer * 10), 0.0f, alpha);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(position.x, position.y);
        for (int i = 0; i <= 20; i++) {
            float theta = 2.0f * M_PI * i / 20;
            float r = radius * (0.8f + 0.2f * sin(timer * 20));
            glVertex2f(position.x + r * cos(theta), position.y + r * sin(theta));
        }
        glEnd();
        
        // Vẽ các hạt
        glPointSize(3);
        glBegin(GL_POINTS);
        for (const auto& p : particles) {
            glColor4f(1.0f, 0.8f, 0.0f, alpha);
            glVertex2f(position.x + p.x * timer, position.y + p.y * timer);
        }
        glEnd();
    }
};

#endif