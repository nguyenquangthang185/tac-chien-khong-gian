#include "SatelliteWarfare.h"
#include <iostream>
#include <sstream>

// Biến toàn cục
Satellite* g_satellite = nullptr;
std::vector<Target*> g_targets;
Missile* g_missile = nullptr;
Explosion* g_explosion = nullptr;

GameState g_gameState = STATE_IDLE;
Vector2D g_selectedTarget;
int g_selectedTargetIndex = -1;

float g_lastTime = 0;
bool g_mousePressed = false;

// Font cho text
void renderText(float x, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

// Vẽ lưới radar
void drawRadarGrid() {
    glColor4f(0.2f, 0.2f, 0.2f, 0.5f);
    glLineWidth(1);
    
    // Vẽ lưới tọa độ
    glBegin(GL_LINES);
    for (int i = 0; i <= SCREEN_WIDTH; i += 50) {
        glVertex2f(i, 0);
        glVertex2f(i, SCREEN_HEIGHT);
    }
    for (int i = 0; i <= SCREEN_HEIGHT; i += 50) {
        glVertex2f(0, i);
        glVertex2f(SCREEN_WIDTH, i);
    }
    glEnd();
    
    // Vẽ vòng tròn radar
    glColor4f(0.0f, 1.0f, 0.0f, 0.3f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 360; i += 5) {
        float angle = i * M_PI / 180;
        float x = SCREEN_WIDTH/2 + 300 * cos(angle);
        float y = SCREEN_HEIGHT/2 + 300 * sin(angle);
        glVertex2f(x, y);
    }
    glEnd();
}

// Vẽ thông tin trạng thái
void drawStatusInfo() {
    std::stringstream ss;
    ss << "Trạng thái: ";
    
    switch(g_gameState) {
        case STATE_IDLE:
            ss << "Chờ chọn mục tiêu";
            glColor3fv(COLOR_WHITE);
            break;
        case STATE_TARGET_LOCKED:
            ss << "Đã khóa mục tiêu - Nhấn SPACE để phóng";
            glColor3fv(COLOR_YELLOW);
            break;
        case STATE_MISSILE_LAUNCHED:
            ss << "Tên lửa đang bay";
            glColor3fv(COLOR_GREEN);
            break;
        case STATE_EXPLOSION:
            ss << "Mục tiêu đã bị tiêu diệt";
            glColor3fv(COLOR_RED);
            break;
    }
    
    renderText(20, SCREEN_HEIGHT - 30, ss.str().c_str());
    
    if (g_selectedTargetIndex >= 0 && g_selectedTargetIndex < g_targets.size()) {
        Target* t = g_targets[g_selectedTargetIndex];
        Vector2D pos = t->getPosition();
        
        ss.str("");
        ss << "Tọa độ mục tiêu: (" << (int)pos.x << ", " << (int)pos.y << ")";
        renderText(20, SCREEN_HEIGHT - 60, ss.str().c_str());
        
        if (t->isLocked()) {
            renderText(20, SCREEN_HEIGHT - 90, "Mục tiêu đã được khóa");
        }
    }
    
    // Hướng dẫn
    glColor3fv(COLOR_GRAY);
    renderText(20, 60, "Click chuột: Chọn mục tiêu");
    renderText(20, 40, "SPACE: Phóng tên lửa");
    renderText(20, 20, "R: Reset mô phỏng");
}

// Khởi tạo các đối tượng
void initSimulation() {
    srand(time(NULL));
    
    // Tạo vệ tinh
    g_satellite = new Satellite(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 200, 0.3f);
    
    // Tạo các mục tiêu ngẫu nhiên
    for (int i = 0; i < 5; i++) {
        float x = 200 + rand() % (SCREEN_WIDTH - 400);
        float y = 200 + rand() % (SCREEN_HEIGHT - 400);
        g_targets.push_back(new Target(x, y));
    }
    
    g_gameState = STATE_IDLE;
    g_selectedTargetIndex = -1;
    g_missile = nullptr;
    g_explosion = nullptr;
}

// Reset mô phỏng
void resetSimulation() {
    delete g_satellite;
    for (auto t : g_targets) delete t;
    g_targets.clear();
    delete g_missile;
    delete g_explosion;
    
    initSimulation();
}

// Hàm vẽ chính
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Vẽ background
    glClearColor(0.0f, 0.0f, 0.05f, 1.0f);
    
    // Vẽ lưới radar
    drawRadarGrid();
    
    // Vẽ vệ tinh
    if (g_satellite) g_satellite->render();
    
    // Vẽ các mục tiêu
    for (auto target : g_targets) {
        target->render();
    }
    
    // Vẽ tên lửa
    if (g_missile && g_missile->isActive()) {
        g_missile->render();
    }
    
    // Vẽ hiệu ứng nổ
    if (g_explosion && g_explosion->isActive()) {
        g_explosion->render();
    }
    
    // Vẽ thông tin
    drawStatusInfo();
    
    glutSwapBuffers();
}

// Cập nhật logic
void update(int value) {
    float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float deltaTime = currentTime - g_lastTime;
    g_lastTime = currentTime;
    
    if (deltaTime > 0.1f) deltaTime = 0.1f; // Giới hạn delta time
    
    // Cập nhật các đối tượng
    if (g_satellite) g_satellite->update(deltaTime);
    
    for (auto target : g_targets) {
        target->update(deltaTime);
    }
    
    // Xử lý tên lửa
    if (g_missile && g_missile->isActive()) {
        g_missile->update(deltaTime);
        
        if (g_missile->hasHitTarget() && g_selectedTargetIndex >= 0) {
            // Tạo hiệu ứng nổ
            Vector2D targetPos = g_targets[g_selectedTargetIndex]->getPosition();
            g_explosion = new Explosion(targetPos.x, targetPos.y);
            
            // Hủy mục tiêu
            g_targets[g_selectedTargetIndex]->destroy();
            delete g_missile;
            g_missile = nullptr;
            g_gameState = STATE_EXPLOSION;
        }
    }
    
    // Cập nhật hiệu ứng nổ
    if (g_explosion && g_explosion->isActive()) {
        g_explosion->update(deltaTime);
    } else if (g_gameState == STATE_EXPLOSION) {
        // Kết thúc hiệu ứng nổ
        g_gameState = STATE_IDLE;
        g_selectedTargetIndex = -1;
    }
    
    glutPostRedisplay();
    glutTimerFunc(16, update, 0); // ~60 FPS
}

// Xử lý click chuột
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Chuyển đổi tọa độ
        y = SCREEN_HEIGHT - y;
        
        // Tìm mục tiêu gần nhất với click
        float minDist = 50; // Khoảng cách tối đa để chọn
        int closestTarget = -1;
        
        for (size_t i = 0; i < g_targets.size(); i++) {
            if (!g_targets[i]->isActive() || g_targets[i]->isDestroyed()) continue;
            
            Vector2D pos = g_targets[i]->getPosition();
            float dist = sqrt(pow(pos.x - x, 2) + pow(pos.y - y, 2));
            
            if (dist < minDist) {
                minDist = dist;
                closestTarget = i;
            }
        }
        
        if (closestTarget >= 0) {
            // Bỏ khóa mục tiêu cũ
            if (g_selectedTargetIndex >= 0 && g_selectedTargetIndex < g_targets.size()) {
                g_targets[g_selectedTargetIndex]->setLocked(false);
            }
            
            // Khóa mục tiêu mới
            g_selectedTargetIndex = closestTarget;
            g_targets[g_selectedTargetIndex]->setLocked(true);
            g_gameState = STATE_TARGET_LOCKED;
        }
    }
}

// Xử lý bàn phím
void keyboard(unsigned char key, int x, int y) {
    switch(key) {
        case ' ': // Phóng tên lửa
            if (g_gameState == STATE_TARGET_LOCKED && g_selectedTargetIndex >= 0) {
                Vector2D satellitePos = g_satellite->getPosition();
                Vector2D targetPos = g_targets[g_selectedTargetIndex]->getPosition();
                
                g_missile = new Missile(satellitePos.x, satellitePos.y, 
                                       targetPos.x, targetPos.y);
                g_gameState = STATE_MISSILE_LAUNCHED;
            }
            break;
            
        case 'r':
        case 'R':
            resetSimulation();
            break;
            
        case 27: // ESC
            exit(0);
            break;
    }
}

// Xử lý resize cửa sổ
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    // Khởi tạo GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    glutCreateWindow(WINDOW_TITLE);
    
    // Thiết lập callback functions
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    
    // Khởi tạo mô phỏng
    initSimulation();
    
    // Bắt đầu vòng lặp chính
    glutTimerFunc(0, update, 0);
    glutMainLoop();
    
    return 0;
}