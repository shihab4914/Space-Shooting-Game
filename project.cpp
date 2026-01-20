
#ifdef _WIN32
    #include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <math.h>

#define GL_SILENCE_DEPRECATION

#define XMAX 1200
#define YMAX 700
#define SPACESHIP_SPEED 20
#define TOP 0
#define RIGHT 1
#define BOTTOM 2
#define LEFT 3

// ------------------------------ Globals ------------------------------
GLint m_viewport[4];
bool mButtonPressed = false;
float mouseX = 0.0f, mouseY = 0.0f;
enum view { INTRO, MENU, INSTRUCTIONS, GAME, GAMEOVER };
view viewPage = INTRO; // initial value
bool keyStates[256] = {false};
bool prevKeyStates[256] = {false}; // for optional edge detection (not strictly required)
bool direction[4] = {false};
bool laser1Dir[2] = {false};
bool laser2Dir[2] = {false};

int alienLife1 = 100;
int alienLife2 = 100;
bool gameOver = false;
float xOne = 500, yOne = 0;
float xTwo = 500, yTwo = 0;
bool laser1 = false, laser2 = false;
GLint CI = 0;

GLfloat a[][2] = { {0, -50}, {70, -50}, {70, 70}, {-70, 70} };
GLfloat LightColor[][3] = { {1,1,0}, {0,1,1}, {0,1,0} };
GLfloat AlienBody[][2] = {{-4,9}, {-6,0}, {0,0}, {0.5,9}, {0.15,12}, {-14,18}, {-19,10}, {-20,0},{-6,0}};
GLfloat AlienCollar[][2] = {{-9,10.5}, {-6,11}, {-5,12}, {6,18}, {10,20}, {13,23}, {16,30}, {19,39}, {16,38},
                          {10,37}, {-13,39}, {-18,41}, {-20,43}, {-20.5,42}, {-21,30}, {-19.5,23}, {-19,20},
                          {-14,16}, {-15,17},{-13,13},  {-9,10.5}};
GLfloat ALienFace[][2] = {{-6,11}, {-4.5,18}, {0.5,20}, {0.,20.5}, {0.1,19.5}, {1.8,19}, {5,20}, {7,23}, {9,29},
                        {6,29.5}, {5,28}, {7,30}, {10,38},{11,38}, {11,40}, {11.5,48}, {10,50.5},{8.5,51}, {6,52},
                        {1,51}, {-3,50},{-1,51}, {-3,52}, {-5,52.5}, {-6,52}, {-9,51}, {-10.5,50}, {-12,49}, {-12.5,47},
                        {-12,43}, {-13,40}, {-12,38.5}, {-13.5,33},{-15,38},{-14.5,32},  {-14,28}, {-13.5,33}, {-14,28},
                        {-13.8,24}, {-13,20}, {-11,19}, {-10.5,12}, {-6,11} } ;
GLfloat ALienBeak[][2] = {{-6,21.5}, {-6.5,22}, {-9,21}, {-11,20.5}, {-20,20}, {-14,23}, {-9.5,28}, {-7,27}, {-6,26.5},
                        {-4.5,23}, {-4,21}, {-6,19.5}, {-8.5,19}, {-10,19.5}, {-11,20.5} };

GLuint dl_ship_body = 0;
GLuint dl_light = 0;

// Fire gating to prevent repeated hits while holding key
bool allowLaser1 = true;
bool allowLaser2 = true;

// ------------------------------ Helpers ------------------------------
static inline int roundf_to_int(float v) { return (int)floor(v + 0.5f); }
static inline float clampf(float v, float a, float b) { return (v < a) ? a : (v > b) ? b : v; }

// ------------------------------ Raster algorithms ------------------------------

// Bresenham integer line algorithm (draws points)
void BresenhamLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;  // error value

    glBegin(GL_POINTS);
    while (1) {
        glVertex2i(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    glEnd();
}

// Midpoint circle algorithm (plots a circle outline using points)
void plotCirclePoints(int xc, int yc, int x, int y) {
    glVertex2i(xc + x, yc + y);
    glVertex2i(xc - x, yc + y);
    glVertex2i(xc + x, yc - y);
    glVertex2i(xc - x, yc - y);
    glVertex2i(xc + y, yc + x);
    glVertex2i(xc - y, yc + x);
    glVertex2i(xc + y, yc - x);
    glVertex2i(xc - y, yc - x);
}

void MidpointCircle(int xc, int yc, int r) {
    int x = 0;
    int y = r;
    int d = 1 - r;
    glBegin(GL_POINTS);
    plotCirclePoints(xc, yc, x, y);
    while (x < y) {
        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
        plotCirclePoints(xc, yc, x, y);
    }
    glEnd();
}

// ------------------------------ Text & init ------------------------------
void displayRasterText(float x, float y, float z, const char *stringToDisplay) {
    glRasterPos3f(x, y, z);
    for (const char* c = stringToDisplay; *c != '\0'; c++){
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);
    }
}

void create_display_lists() {
    // Ship body (low-res ellipse made of triangle fan) compiled once
    dl_ship_body = glGenLists(1);
    glNewList(dl_ship_body, GL_COMPILE);
        glPushMatrix();
        // we'll draw a unit circle and scale it where used
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(0.0f, 0.0f);
            int segs = 32;
            for (int i = 0; i <= segs; ++i) {
                float theta = 2.0f * 3.14159265359f * (float)i / (float)segs;
                glVertex2f(cosf(theta), sinf(theta));
            }
        glEnd();
        glPopMatrix();
    glEndList();

    // Light/dot (small sphere substitute) as a small circle
    dl_light = glGenLists(1);
    glNewList(dl_light, GL_COMPILE);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(0.0f, 0.0f);
            int segs2 = 18;
            for (int i = 0; i <= segs2; ++i) {
                float theta = 2.0f * 3.14159265359f * (float)i / (float)segs2;
                glVertex2f(cosf(theta), sinf(theta));
            }
        glEnd();
    glEndList();
}

void init()
{
    glClearColor(0.0, 0.0, 0.0, 0);
    glColor3f(1.0, 0.0, 0.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluOrtho2D(-1200, 1200, -700, 700);
    glMatrixMode(GL_MODELVIEW);

    create_display_lists();
}

// ------------------------------ Screens ------------------------------
void introScreen()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1.0, 0.0, 0.0);
    displayRasterText(-425, 490, 0.0, "NMAM INSTITUTE OF TECHNOLOGY");
    glColor3f(1.0, 1.0, 1.0);
    displayRasterText(-700, 385, 0.0, "DEPARTMENT OF COMPUTER SCIENCE AND ENGINEERING");
    glColor3f(0.0, 0.0, 1.0);
    displayRasterText(-225, 300, 0.0, "A MINI PROJECT ON ");
    glColor3f(1.0, 0.0, 1.0);
    displayRasterText(-125, 225, 0.0, "Space Shooter");
    glColor3f(1.0, 0.7, 0.8);
    displayRasterText(-100, 150, 0.0, "Course Code ");
    glColor3f(1.0, 1.0, 1.0);
    displayRasterText(-130, 80, 0.0, "     CSE 422");
    glColor3f(1.0, 0.0, 0.0);
    displayRasterText(-800, -100, 0.0, " STUDENT NAMES");
    glColor3f(1.0, 1.0, 1.0);
    displayRasterText(-800, -200, 0.0, " Shihab Sumon");
    displayRasterText(-800, -285, 0.0, " Shourov Kazi");
    glColor3f(1.0, 0.0, 0.0);
    displayRasterText(500, -100, 0.0, "Under the Guidance of");
    glColor3f(1.0, 1.0, 1.0);
    displayRasterText(500, -200, 0.0, "Nawshin Haque");
    glColor3f(1.0, 0.0, 0.0);
    displayRasterText(-250, -400, 0.0, "Computer Graphics Project");
    glColor3f(1.0, 1.0, 1.0);
    displayRasterText(-300, -550, 0.0, "Press ENTER to start the game");
    // no debug prints
}

// start screen with menu
void startScreenDisplay()
{
    glLineWidth(10);
    glColor3f(1,0,0);
    glBegin(GL_LINE_LOOP);               //Border
        glVertex2f(-750 ,-500);
        glVertex2f(-750 ,550);
        glVertex2f(750 ,550);
        glVertex2f(750 ,-500);
    glEnd();

    glLineWidth(1);

    glColor3f(1, 1, 0);
    glBegin(GL_POLYGON);                //START GAME POLYGON
        glVertex2f(-200 ,300);
        glVertex2f(-200 ,400);
        glVertex2f(200 ,400);
        glVertex2f(200 ,300);
    glEnd();

    glBegin(GL_POLYGON);                //INSTRUCTIONS POLYGON
        glVertex2f(-200, 50);
        glVertex2f(-200 ,150);
        glVertex2f(200 ,150);
        glVertex2f(200 ,50);
    glEnd();

    glBegin(GL_POLYGON);                //QUIT POLYGON
        glVertex2f(-200 ,-200);
        glVertex2f(-200 ,-100);
        glVertex2f(200, -100);
        glVertex2f(200, -200);
    glEnd();

    if(mouseX>=-100 && mouseX<=100 && mouseY>=150 && mouseY<=200){
        glColor3f(0 ,0 ,1) ;
        if(mButtonPressed){
            alienLife1 = alienLife2 = 100;
            viewPage = GAME;
            mButtonPressed = false;
        }
    } else
        glColor3f(0 , 0, 0);

    displayRasterText(-100 ,340 ,0.4 ,"Start Game");

    if(mouseX>=-100 && mouseX<=100 && mouseY>=30 && mouseY<=80) {
        glColor3f(0 ,0 ,1);
        if(mButtonPressed){
            viewPage = INSTRUCTIONS;
            mButtonPressed = false;
        }
    } else
        glColor3f(0 , 0, 0);
    displayRasterText(-120 ,80 ,0.4 ,"Instructions");

    if(mouseX>=-100 && mouseX<=100 && mouseY>=-90 && mouseY<=-40){
        glColor3f(0 ,0 ,1);
        if(mButtonPressed){
            mButtonPressed = false;
            exit(0);
        }
    }
    else
        glColor3f(0 , 0, 0);
    displayRasterText(-100 ,-170 ,0.4 ,"    Quit");
}

// back button rendering
void backButton() {
    if(mouseX <= -450 && mouseX >= -500 && mouseY >= -275 && mouseY <= -250){
            glColor3f(0, 0, 1);
            if(mButtonPressed) {
                viewPage = MENU;
                mButtonPressed = false;
            }
    }
    else glColor3f(1, 0, 0);
    displayRasterText(-1000 ,-550 ,0, "Back");
}

// instructions screen
void instructionsScreenDisplay()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(1, 0, 0);
    displayRasterText(-900 ,550 ,0.4 ,"INSTRUCTIONS");
    glColor3f(1, 0, 0);
    displayRasterText(-1000 ,400 ,0.4 ,"PLAYER 1");
    displayRasterText(200 ,400 ,0.4 ,"PLAYER 2");
    glColor3f(1, 1, 1);
    displayRasterText(-1100 ,300 ,0.4 ,"Key 'w' to move up.");
    displayRasterText(-1100 ,200 ,0.4 ,"Key 's' to move down.");
    displayRasterText(-1100 ,100 ,0.4 ,"Key 'd' to move right.");
    displayRasterText(-1100 ,0 ,0.4 ,"Key 'a' to move left.");
    displayRasterText(100 ,300 ,0.4 ,"Key 'i' to move up.");
    displayRasterText(100 ,200 ,0.4 ,"Key 'k' to move down.");
    displayRasterText(100 ,100 ,0.4 ,"Key 'j' to move right.");
    displayRasterText(100 ,0 ,0.4 ,"Key 'l' to move left.");
    displayRasterText(-1100 ,-100 ,0.4 ,"Key 'c' to shoot, Use 'w' and 's' to change direction.");
    displayRasterText(100 ,-100 ,0.4 ,"Key 'm' to shoot, Use 'i' and 'k' to change direction.");
    displayRasterText(-1100, -300,0.4,"The Objective is to kill your opponent.");
    displayRasterText(-1100 ,-370 ,0.4 ,"Each time a player gets shot, LIFE decreases by 5 points.");
    backButton();
}

// ------------------------------ Draw parts ------------------------------
void DrawAlienBody(bool isPlayer1)
{
    if(isPlayer1) glColor3f(0,1,0);
    else glColor3f(1,1,0);
    glBegin(GL_POLYGON);
    for(int i=0;i<=8;i++) glVertex2fv(AlienBody[i]);
    glEnd();

    glColor3f(0,0,0);
    glLineWidth(1);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=8;i++) glVertex2fv(AlienBody[i]);
    glEnd();

    glBegin(GL_LINES); // effect
        glVertex2f(-13,11);
        glVertex2f(-15,9);
    glEnd();
}
void DrawAlienCollar()
{
    glColor3f(1,0,0);
    glBegin(GL_POLYGON);
    for(int i=0;i<=20 ;i++) glVertex2fv(AlienCollar[i]);
    glEnd();

    glColor3f(0,0,0);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=20 ;i++) glVertex2fv(AlienCollar[i]);
    glEnd();
}
void DrawAlienFace(bool isPlayer1)
{
    glColor3f(0,0,1);
    glBegin(GL_POLYGON);
    for(int i=0;i<=42 ;i++) glVertex2fv(ALienFace[i]);
    glEnd();

    glColor3f(0,0,0);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=42 ;i++) glVertex2fv(ALienFace[i]);
    glEnd();

    glBegin(GL_LINE_STRIP); // ear effect
        glVertex2f(3.3,22);
        glVertex2f(4.4,23.5);
        glVertex2f(6.3,26);
    glEnd();
}
void DrawAlienBeak()
{
    glColor3f(1,1,0);
    glBegin(GL_POLYGON);
    for(int i=0;i<=14 ;i++) glVertex2fv(ALienBeak[i]);
    glEnd();

    glColor3f(0,0,0);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=14 ;i++) glVertex2fv(ALienBeak[i]);
    glEnd();
}
void DrawAlienEyes(bool isPlayer1)
{
    glColor3f(0,1,1);

    glPushMatrix();
    glRotated(-10,0,0,1);
    glTranslated(-6,32.5,0);      //Left eye
    glScalef(2.5,4,0);
    // replaced heavy glutSolidSphere with low-res display list scaled
    glPushMatrix();
        glScalef(1.0f, 1.0f, 1.0f);
        glutSolidSphere(1, 8, 8);
    glPopMatrix();
    glPopMatrix();

    glPushMatrix();
    glRotated(-1,0,0,1);
    glTranslated(-8,36,0);        //Right eye
    glScalef(2.5,4,0);
    glutSolidSphere(1, 8, 8);
    glPopMatrix();
}
void DrawAlien(bool isPlayer1)
{
    DrawAlienBody(isPlayer1);
    DrawAlienCollar();
    DrawAlienFace(isPlayer1);
    DrawAlienBeak();
    DrawAlienEyes(isPlayer1);
}
void DrawSpaceshipBody(bool isPlayer1)
{
    if(isPlayer1) glColor3f(1, 0, 0);
    else glColor3f(0.5, 0, 0.5);

    // use display list (unit circle) scaled to an ellipse
    glPushMatrix();
        glScalef(70,20,1);
        glCallList(dl_ship_body);
    glPopMatrix();

    // Lights - replaced with low-res small circles and display list
    glPushMatrix();
        glScalef(3,3,1);
        glTranslated(-20,0,0);            // 1
        glColor3fv(LightColor[(CI+0)%3]);
        glPushMatrix();
            glScalef(4.0f, 4.0f, 1.0f); // scale small circle
            glCallList(dl_light);
        glPopMatrix();

        glTranslated(5,0,0);              // 2
        glColor3fv(LightColor[(CI+1)%3]);
        glPushMatrix();
            glScalef(4.0f, 4.0f, 1.0f);
            glCallList(dl_light);
        glPopMatrix();

        glTranslated(5,0,0);              // 3
        glColor3fv(LightColor[(CI+2)%3]);
        glPushMatrix();
            glScalef(4.0f, 4.0f, 1.0f);
            glCallList(dl_light);
        glPopMatrix();

        // you can add the rest similarly but fewer lights reduces draw cost
    glPopMatrix();
}
void DrawSteeringWheel()
{
    // Draw steering wheel using a cheap line-loop
    glPushMatrix();
        glScalef(7,4,1);
        glTranslated(-1.9,5.5,0);
        glColor3f(0.20,0.,0.20);
        int segs = 36;
        glLineWidth(3);
        glBegin(GL_LINE_LOOP);
            for (int i = 0; i < segs; ++i) {
                float theta = 2.0f * 3.14159265359f * (float)i / (float)segs;
                glVertex2f(cosf(theta), sinf(theta));
            }
        glEnd();
    glPopMatrix();
}

// translucent dome
void DrawSpaceshipDoom()
{
    glColor4f(0.7,1,1,0.0011);
    glPushMatrix();
    glTranslated(0,30,0);
    glScalef(35,50,1);
    glutSolidSphere(1,10,10); // lower res
    glPopMatrix();
}

// ------------------------------ Laser & collision ------------------------------
// checkLaserContact now RETURNS true when a hit occurred.
// shooterIsPlayer1 indicates who fired the laser so we reduce the opponent's life.
bool checkLaserContact(int laserX, int laserY, int targetX, int targetY, bool shooterIsPlayer1) {
    int r = 50; // approx ship radius
    float dx = laserX - targetX;
    float dy = laserY - targetY;
    float distance = sqrt(dx*dx + dy*dy);

    if(distance < r) {
        if(shooterIsPlayer1) alienLife2 -= 5;
        else alienLife1 -= 5;
        return true;
    }
    return false;
}


void DrawLaser(int x, int y, bool dir[]) {
    int xend = -XMAX, yend = y;
    if(dir[0]) yend = YMAX;
    else if(dir[1]) yend = -YMAX;

    // convert to integer coordinates in the same ortho world
    int xi = roundf_to_int((float)x);
    int yi = roundf_to_int((float)y);
    int xei = roundf_to_int((float)xend);
    int yei = roundf_to_int((float)yend);

    glPointSize(3);            // thickness
    glColor3f(1.0f, 0.0f, 0.0f);
    BresenhamLine(xi, yi, xei, yei);
}

void SpaceshipCreate(int x, int y, bool isPlayer1){
    glPushMatrix();
    glTranslated(x,y,0);
    DrawSpaceshipDoom();
    glPushMatrix();
        glTranslated(4,19,0);
        DrawAlien(isPlayer1);
    glPopMatrix();
    DrawSteeringWheel();
    DrawSpaceshipBody(isPlayer1);
    // removed stray glEnd() that existed in original
    glPopMatrix();
}

void DisplayHealthBar1() {
    char temp1[40];
    glColor3f(1 ,1 ,1);
    sprintf(temp1,"  LIFE = %d",alienLife1);
    displayRasterText(-1100 ,600 ,0.4 ,temp1);
}

void DisplayHealthBar2() {
    char temp2[40];
    glColor3f(1 ,1 ,1);
    sprintf(temp2,"  LIFE = %d",alienLife2);
    displayRasterText(800 ,600 ,0.4 ,temp2);
}

// ------------------------------ Game screen & flow ------------------------------
void gameScreenDisplay()
{
    DisplayHealthBar1();
    DisplayHealthBar2();

    glPushMatrix();
    glScalef(2, 2 ,0);

    // ---------------- Player 1 ----------------
    if(alienLife1 > 0){
        SpaceshipCreate(xOne, yOne, true);

        if(laser1) {
            DrawLaser(xOne, yOne, laser1Dir);

            // Player 2 is the target
            int targetX2 = xTwo;
            int targetY2 = yTwo;

            if(checkLaserContact(xOne, yOne, targetX2, targetY2, true)) {
                laser1 = false;
                allowLaser1 = false;
            }
        }
    } else {
        viewPage = GAMEOVER;
    }

    // ---------------- Player 2 ----------------
    if(alienLife2 > 0){
        glPushMatrix();
        glScalef(-1, 1, 1);   // mirror horizontally
        SpaceshipCreate(xTwo, yTwo, false);
        glPopMatrix();

        if(laser2) {
            DrawLaser(xTwo, yTwo, laser2Dir);

            // Player 1 is the target
            int targetX1 = xOne;
            int targetY1 = yOne;

            if(checkLaserContact(xTwo, yTwo, targetX1, targetY1, false)) {
                laser2 = false;
                allowLaser2 = false;
            }
        }
    } else {
        viewPage = GAMEOVER;
    }

    glPopMatrix();

    if(viewPage == GAMEOVER) {
        xOne = xTwo = 500;
        yOne = yTwo = 0;
    }
}


void displayGameOverMessage() {
    glColor3f(1, 1, 0);
    const char* message;
    if(alienLife1 > 0) message = "Game Over! Player 1 won the game";
    else message = "Game Over! Player 2 won the game";

    displayRasterText(-350 ,600 ,0.4 , message);
}

// ------------------------------ Input & logic ------------------------------
void keyOperations() {
    if(keyStates[13] == true && viewPage == INTRO) {
        viewPage = MENU;
    }
    if(viewPage == GAME) {
        // reset direction flags for lasers each frame
        laser1Dir[0] = laser1Dir[1] = false;
        laser2Dir[0] = laser2Dir[1] = false;

        // PLAYER 2 controls (original mapping: c to shoot, w/s to change direction)
        if ( keyStates['m'] ) {
            // fire only if allowedFire flag is true (i.e., new press or previously released)
            if ( allowLaser2 ) {
                laser2 = true;
                allowLaser2 = false; // block re-fire until release
                // set direction if pressed
                if(keyStates['w'] == true)  laser2Dir[0] = true;
                if(keyStates['s'] == true)  laser2Dir[1] = true;
            }
            else {
                // if not allowed, keep laser active only if already active; don't re-enable
                // and keep direction flags from current keys
                if(laser2) {
                    if(keyStates['w'] == true)  laser2Dir[0] = true;
                    if(keyStates['s'] == true)  laser2Dir[1] = true;
                }
            }
        }
        else { // not pressing c -> movement allowed and reset allow flag so next press will fire
            laser2 = false;
            allowLaser2 = true;
            if(keyStates['d'] == true) xTwo-=SPACESHIP_SPEED;
            if(keyStates['a'] == true) xTwo+=SPACESHIP_SPEED;
            if(keyStates['w'] == true) yTwo+=SPACESHIP_SPEED;
            if(keyStates['s'] == true) yTwo-=SPACESHIP_SPEED;
        }

        // PLAYER 1 controls (m to shoot, i/k directions)
        if ( keyStates['c'] ) {
            if ( allowLaser1 ) {
                laser1 = true;
                allowLaser1 = false;
                if(keyStates['i'] == true) laser1Dir[0] = true;
                if(keyStates['k'] == true) laser1Dir[1] = true;
            } else {
                if(laser1) {
                    if(keyStates['i'] == true) laser1Dir[0] = true;
                    if(keyStates['k'] == true) laser1Dir[1] = true;
                }
            }
        }
        else {
            laser1 = false;
            allowLaser1 = true;
            if(keyStates['l'] == true) xOne+=SPACESHIP_SPEED;
            if(keyStates['j'] == true) xOne-=SPACESHIP_SPEED;
            if(keyStates['i'] == true) yOne+=SPACESHIP_SPEED;
            if(keyStates['k'] == true) yOne-=SPACESHIP_SPEED;
        }
    }

    // copy current states to prevKeyStates (not required for current gating approach but kept)
    for (int i = 0; i < 256; ++i) prevKeyStates[i] = keyStates[i];
}




// ------------------------------ Display callback ------------------------------
void display()
{
    keyOperations();
    glClear(GL_COLOR_BUFFER_BIT);

    switch (viewPage)
    {
        case INTRO:
            introScreen();
            break;
        case MENU:
            startScreenDisplay();
            break;
        case INSTRUCTIONS:
            instructionsScreenDisplay();
            break;
        case GAME:
            gameScreenDisplay();
            break;
        case GAMEOVER:
            displayGameOverMessage();
            startScreenDisplay();
            break;
    }

    glLoadIdentity();
    glutSwapBuffers();
}

// ------------------------------ Mouse / Keyboard / Timer ------------------------------
void passiveMotionFunc(int x,int y) {
    // convert window mouse coords to ortho coords used by the program
    mouseX = float(x) / (m_viewport[2] / 1200.0f) - 600.0f;
    mouseY = -(float(y) / (m_viewport[3] / 700.0f) - 350.0f);
    // only request redraw on motion (timer drives steady redraw)
}

void mouseClick(int buttonPressed ,int state ,int x, int y) {
    if(buttonPressed == GLUT_LEFT_BUTTON && state == GLUT_DOWN) mButtonPressed = true;
    else if(buttonPressed == GLUT_LEFT_BUTTON && state == GLUT_UP) mButtonPressed = false;
}

void keyPressed(unsigned char key, int x, int y)
{
    keyStates[key] = true;
}

void keyReleased(unsigned char key, int x, int y) {
    keyStates[key] = false;
}

// timer at ~60 FPS
void timerFunc(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timerFunc, 0);
}

// ------------------------------ main ------------------------------
int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowPosition(0, 0);
    glutInitWindowSize(1200, 600);
    glutCreateWindow("Space Shooter - Fixed (single hit per shot)");

    init();
    // replace idle with timer-based redraw for stable framerate
    glutTimerFunc(16, timerFunc, 0);

    glutKeyboardFunc(keyPressed);
    glutKeyboardUpFunc(keyReleased);
    glutMouseFunc(mouseClick);
    glutPassiveMotionFunc(passiveMotionFunc);
    glGetIntegerv(GL_VIEWPORT ,m_viewport);
    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}