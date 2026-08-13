#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "input.h"
#include "render.h"
#include "life.h"

int gSelectedPattern = 4;
int gMenuMode = MENU_MODE_MAIN;
int gActiveField = 0;
int gSizeConfirmRequested = 0;
char gSizeX[8] = "50";
char gSizeY[8] = "50";

struct input_state Input = {
    .zoom = 1.0f,
    .cameraX = 0.0f,
    .cameraY = 0.0f,
    .lastUpdateTime = 0.0,
    .updateInterval = 0.1,
    .freeze = 0,
    .isFullscreen = 0,
    .windowedX = 0,
    .windowedY = 0,
    .windowedWidth = 0,
    .windowedHeight = 0,
};

static void appendDigit(char* buffer, size_t capacity, char digit) {
    size_t len = strlen(buffer);
    if (len >= 3) {
        return;
    }
    if (len + 1 < capacity) {
        if (len == 1 && buffer[0] == '0') {
            buffer[0] = digit;
            return;
        }
        buffer[len] = digit;
        buffer[len + 1] = '\0';
    }
}

static void removeLastDigit(char* buffer) {
    size_t len = strlen(buffer);
    if (len > 0) {
        buffer[len - 1] = '\0';
    }
    if (buffer[0] == '\0') {
        buffer[0] = '0';
        buffer[1] = '\0';
    }
}

void updateMenuTitle(GLFWwindow* window) {
    glfwSetWindowTitle(window, "Game of Life");
}

void menuFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

void menuKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;

    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }

    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        Input.isFullscreen = !Input.isFullscreen;
        
        if (Input.isFullscreen) {
            glfwGetWindowPos(window, &Input.windowedX, &Input.windowedY);
            glfwGetWindowSize(window, &Input.windowedWidth, &Input.windowedHeight);
            
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(window, NULL, Input.windowedX, Input.windowedY, Input.windowedWidth, Input.windowedHeight, 0);
        }
        return;
    }

    if (gMenuMode == MENU_MODE_MAIN) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, 1);
        }
        return;
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        gActiveField = 1 - gActiveField;
    } else if (key == GLFW_KEY_BACKSPACE) {
        if (gActiveField == 0) {
            removeLastDigit(gSizeX);
        } else {
            removeLastDigit(gSizeY);
        }
    } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
        gSizeConfirmRequested = 1;
    } else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        gMenuMode = MENU_MODE_MAIN;
    }
}

void menuCharCallback(GLFWwindow* window, unsigned int codepoint) {
    (void)window;
    if (gMenuMode != MENU_MODE_SIZE_INPUT) {
        return;
    }
    if (codepoint >= '0' && codepoint <= '9') {
        if (gActiveField == 0) {
            appendDigit(gSizeX, sizeof(gSizeX), (char)codepoint);
        } else {
            appendDigit(gSizeY, sizeof(gSizeY), (char)codepoint);
        }
    }
}

static float clampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static void clampCameraToGrid(float aspect) {
    const struct grid* grid = &Render.grid;
    const float edgePaddingRatio = 0.12f;

    float padX = (grid->right - grid->left) * edgePaddingRatio;
    float padY = (grid->top - grid->bot) * edgePaddingRatio;

    float extendedLeft = grid->left - padX;
    float extendedRight = grid->right + padX;
    float extendedBot = grid->bot - padY;
    float extendedTop = grid->top + padY;

    float halfWidth = aspect * Input.zoom;
    float halfHeight = Input.zoom;

    float minCameraX = extendedLeft + halfWidth;
    float maxCameraX = extendedRight - halfWidth;
    float minCameraY = extendedBot + halfHeight;
    float maxCameraY = extendedTop - halfHeight;

    if (minCameraX > maxCameraX) {
        Input.cameraX = 0.5f * (grid->left + grid->right);
    } else {
        Input.cameraX = clampFloat(Input.cameraX, minCameraX, maxCameraX);
    }

    if (minCameraY > maxCameraY) {
        Input.cameraY = 0.5f * (grid->bot + grid->top);
    } else {
        Input.cameraY = clampFloat(Input.cameraY, minCameraY, maxCameraY);
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mod){
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
        glfwSetWindowShouldClose(window, 1);
    }
    if(key == GLFW_KEY_SPACE && action == GLFW_PRESS){
        Input.freeze = !Input.freeze;
    }
    if(key == GLFW_KEY_P && action == GLFW_PRESS){
        for(int x = 0; x < Life.height; ++x){
            for(int y = 0; y < Life.width; ++y){
                char life = rand() % 3 == 0;
                Life.state[y * Life.width + x].cur = life;
            }
        }
        for (int i = 0; i < Life.width * Life.height; i++) {
            Life.state[i].cycleCount = 0;
        }
    }
    if(key == GLFW_KEY_R && action == GLFW_PRESS){
        for (int i = 0; i < Life.width * Life.height; i++) {
            Life.state[i].cycleCount = 0;
            Life.state[i].cur = 0;
        }
    }
    if(key == GLFW_KEY_Z && action == GLFW_PRESS){
        Input.updateInterval *= (double)2;
        if (Input.updateInterval > 1.6f) Input.updateInterval = 1.6f;
    }
    if(key == GLFW_KEY_X && action == GLFW_PRESS){
        Input.updateInterval /= (double)2;
        if (Input.updateInterval < 0.0125f) Input.updateInterval = 0.0125f;
    }
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        Input.isFullscreen = !Input.isFullscreen;
        
        if (Input.isFullscreen) {
            glfwGetWindowPos(window, &Input.windowedX, &Input.windowedY);
            glfwGetWindowSize(window, &Input.windowedWidth, &Input.windowedHeight);
            
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetWindowMonitor(window, NULL, Input.windowedX, Input.windowedY, Input.windowedWidth, Input.windowedHeight, 0);
        }
    }
}

void buffersizeCallback(GLFWwindow* window, int width, int height){
    glViewport(0, 0, width, height);

    if (height == 0) {
        return;
    }

    float aspect = (float)width / (float)height;
    clampCameraToGrid(aspect);

    float bottom = Input.cameraY - Input.zoom;
    float top = Input.cameraY + Input.zoom;

    float left = Input.cameraX - aspect * Input.zoom;
    float right = Input.cameraX + aspect * Input.zoom;

    float near = -1.0f;
    float far = 1.0f;

    Projection* proj = getProjection();
    makeOrtho(left, right, bottom, top, near, far, proj);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset){
    float zoomSpeed = 0.1f;
    Input.zoom -= (float)yoffset * zoomSpeed;
    if (Input.zoom < 0.1f) Input.zoom = 0.1f;
    if (Input.zoom > 10.0f) Input.zoom = 10.0f;
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    buffersizeCallback(window, width, height);
}

void updateCamera(GLFWwindow* window, float deltaTime) {
    float dirX = 0.0f;
    float dirY = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) dirY += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) dirY -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dirX += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dirX -= 1.0f;

    if (dirX == 0.0f && dirY == 0.0f) {
        return;
    }

    if (dirX != 0.0f && dirY != 0.0f) {
        const float invSqrt2 = 0.70710678f;
        dirX *= invSqrt2;
        dirY *= invSqrt2;
    }

    float moveSpeed = 2.0f * Input.zoom;
    Input.cameraX += dirX * moveSpeed * deltaTime;
    Input.cameraY += dirY * moveSpeed * deltaTime;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    buffersizeCallback(window, width, height);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods){
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {                
            processMouseClick(window, 1);               
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {                
            processMouseClick(window, 0);               
        }                
    }
}

void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {                
        processMouseClick(window, 1);              
    } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {                
        processMouseClick(window, 0);               
    }
}

void processMouseClick(GLFWwindow* window, int action){
    Projection* proj = getProjection();
    const struct grid* grid = &Render.grid;
    int textureWidth = Render.textureWidth;
    int textureHeight = Render.textureHeight;

    double xpos, ypos;                
    glfwGetCursorPos(window, &xpos, &ypos);                

    int windowWidth, windowHeight;
    int framebufferWidth, framebufferHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    if (windowWidth <= 0 || windowHeight <= 0 || framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    float scaledX = (float)xpos * ((float)framebufferWidth / (float)windowWidth);
    float scaledY = (float)ypos * ((float)framebufferHeight / (float)windowHeight);

    float x = (2.0f * scaledX) / (float)framebufferWidth - 1.0f;
    float y = 1.0f - (2.0f * scaledY) / (float)framebufferHeight;

    Projection invProj;                
    invertOrtho(proj, &invProj);                

    float worldX = x * invProj.mat[0] + y * invProj.mat[4] + invProj.mat[12];                
    float worldY = x * invProj.mat[1] + y * invProj.mat[5] + invProj.mat[13];                

    int cellX = (int)((worldX - grid->left) / ((grid->right - grid->left) / textureWidth));
    int cellY = (int)((grid->top - worldY) / ((grid->top - grid->bot) / textureHeight));

    if (cellX >= 0 && cellX < textureWidth && cellY >= 0 && cellY < textureHeight) {                
        int index = cellY * textureWidth + cellX;
        Life.state[index].cur = action;
        Life.state[index].prev = 0;
        Life.state[index].two = 0;
        Life.state[index].cycleCount = 0;             
    }
}

int isFreeze(){
    double currentTime = glfwGetTime();
    if(Input.freeze && currentTime - Input.lastUpdateTime >= Input.updateInterval) {
        Input.lastUpdateTime = currentTime;
        return 0;
    }
    return 1;
}

void setWindowImage(GLFWwindow* window){
    GLFWimage images[3];
    int channels;
    int iconCount = 0;

    unsigned char* icon16 = stbi_load("img/icon16.png", &images[iconCount].width, &images[iconCount].height, &channels, 4);
    if (icon16) {
        images[iconCount].pixels = icon16;
        iconCount++;
    } else {
        fprintf(stderr, "Failed to load icon: img/icon16.png\n");
    }

    unsigned char* icon32 = stbi_load("img/icon32.png", &images[iconCount].width, &images[iconCount].height, &channels, 4);
    if (icon32) {
        images[iconCount].pixels = icon32;
        iconCount++;
    } else {
        fprintf(stderr, "Failed to load icon: img/icon32.png\n");
    }

    unsigned char* icon48 = stbi_load("img/icon48.png", &images[iconCount].width, &images[iconCount].height, &channels, 4);
    if (icon48) {
        images[iconCount].pixels = icon48;
        iconCount++;
    } else {
        fprintf(stderr, "Failed to load icon: img/icon48.png\n");
    }

    if (iconCount > 0) {
        glfwSetWindowIcon(window, iconCount, images);
    }

    for (int i = 0; i < iconCount; ++i) {
        stbi_image_free(images[i].pixels);
    }

}

void setCallback(GLFWwindow* window){
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, buffersizeCallback);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    buffersizeCallback(window, width, height);
    glfwSetScrollCallback(window, scrollCallback);
    glClearColor(0.0f, 0.749f, 1.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}