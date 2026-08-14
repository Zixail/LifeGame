#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "render.h"
#include "life.h"
#include "input.h"
#include <time.h>


int main(void){
    //
    srand(time(NULL));
    //

    if(!glfwInit()){
        printf("glfw error");
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Game of Life", NULL, NULL);
    if(!window){
        printf("window error");
        glfwTerminate();
        return -2;
    }

    setWindowImage(window);
    glfwMakeContextCurrent(window);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        printf("glad error");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -3;
    }

    glfwSwapInterval(1);
    glClearColor(0.0f, 0.749f, 1.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSetFramebufferSizeCallback(window, menuFramebufferSizeCallback);
    glfwSetKeyCallback(window, menuKeyCallback);
    glfwSetCharCallback(window, menuCharCallback);

    programCreate();
    if (Render.shader.program == 0) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -4;
    }

    if (!menuInit()) {
        glDeleteProgram(Render.shader.program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -5;
    }

    double lastFrameTime = glfwGetTime();
    int gameStarted = 0;
    int previousLeftMouseState = GLFW_RELEASE;

    while(!glfwWindowShouldClose(window)){
        double currentFrameTime = glfwGetTime();
        float deltaTime = (float)(currentFrameTime - lastFrameTime);
        lastFrameTime = currentFrameTime;

        glfwPollEvents();                
        glClear(GL_COLOR_BUFFER_BIT);      
        updateMenuTitle(window);

        if (!gameStarted) {
            if (gMenuMode == MENU_MODE_MAIN) {
                renderMenu(gSizeX, gSizeY, 0, 0);

                int currentLeftMouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
                if (currentLeftMouseState == GLFW_PRESS && previousLeftMouseState == GLFW_RELEASE) {
                    double xpos, ypos;
                    glfwGetCursorPos(window, &xpos, &ypos);
                    int width, height;
                    glfwGetWindowSize(window, &width, &height);
                    if (width > 0 && height > 0) {
                        float ndcX = (2.0f * (float)xpos) / (float)width - 1.0f;
                        float ndcY = 1.0f - (2.0f * (float)ypos) / (float)height;
                        int hit = menuHitTestNdc(ndcX, ndcY, 0);
                        if (hit == 1) {
                            gMenuMode = MENU_MODE_SIZE_INPUT;
                            gActiveField = 0;
                        } else if (hit == 2) {
                            glfwSetWindowShouldClose(window, 1);
                        } else if (hit == 5) {
                            gMenuMode = MENU_MODE_TEMPLATES;
                        }
                        else if (hit == 6) {
                            gMenuMode = MENU_MODE_ABOUT;
                        }
                        else if (hit == 7) {
                            gMenuMode = MENU_MODE_FAQ;
                        }
                    }
                }
                previousLeftMouseState = currentLeftMouseState;
            } else if (gMenuMode == MENU_MODE_TEMPLATES) {
                renderTemplatesMenu();

                int currentLeftMouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
                if (currentLeftMouseState == GLFW_PRESS && previousLeftMouseState == GLFW_RELEASE) {
                    double xpos, ypos;
                    glfwGetCursorPos(window, &xpos, &ypos);
                    int width, height;
                    glfwGetWindowSize(window, &width, &height);
                    if (width > 0 && height > 0) {
                        float ndcX = (2.0f * (float)xpos) / (float)width - 1.0f;
                        float ndcY = 1.0f - (2.0f * (float)ypos) / (float)height;
                        int pattern = templatesHitTestNdc(ndcX, ndcY);
                        if (pattern >= 1 && pattern <= 4) {
                            gSelectedPattern = pattern;
                            gMenuMode = MENU_MODE_MAIN;
                        } else {
                            gMenuMode = MENU_MODE_MAIN;
                        }
                    }
                }
                previousLeftMouseState = currentLeftMouseState;
            } else if (gMenuMode == MENU_MODE_SIZE_INPUT) {
                renderMenu(gSizeX, gSizeY, 1, gActiveField);

                int currentLeftMouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
                if (currentLeftMouseState == GLFW_PRESS && previousLeftMouseState == GLFW_RELEASE) {
                    double xpos, ypos;
                    glfwGetCursorPos(window, &xpos, &ypos);
                    int width, height;
                    glfwGetWindowSize(window, &width, &height);
                    if (width > 0 && height > 0) {
                        float ndcX = (2.0f * (float)xpos) / (float)width - 1.0f;
                        float ndcY = 1.0f - (2.0f * (float)ypos) / (float)height;
                        int hit = menuHitTestNdc(ndcX, ndcY, 1);
                        if (hit == 1) {
                            gSizeConfirmRequested = 1;
                        } else if (hit == 2) {
                            gMenuMode = MENU_MODE_MAIN;
                        } else if (hit == 3) {
                            gActiveField = 0;
                        } else if (hit == 4) {
                            gActiveField = 1;
                        }
                    }
                }
                previousLeftMouseState = currentLeftMouseState;

                if (gSizeConfirmRequested) {
                    int fieldWidth = atoi(gSizeX);
                    int fieldHeight = atoi(gSizeY);
                    if (fieldWidth < 5) fieldWidth = 5;
                    if (fieldHeight < 5) fieldHeight = 5;
                    if (fieldWidth > 500) fieldWidth = 500;
                    if (fieldHeight > 500) fieldHeight = 500;

                    initField(fieldWidth, fieldHeight);
                    loadPattern(gSelectedPattern);
                    gridInit();
                    quadInit();
                    setCallback(window);
                    glfwSetWindowTitle(window, "Game of Life");
                    cleanupMenuResources();
                    gameStarted = 1;
                    gSizeConfirmRequested = 0;
                }
            } else if (gMenuMode == MENU_MODE_ABOUT) {
                renderAboutMenu();
                
                int currentLeftMouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
                if (currentLeftMouseState == GLFW_PRESS && previousLeftMouseState == GLFW_RELEASE) {
                    gMenuMode = MENU_MODE_MAIN;
                }
                previousLeftMouseState = currentLeftMouseState;
            } else if (gMenuMode == MENU_MODE_FAQ) {
                renderFaqMenu();

                int currentLeftMouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
                if (currentLeftMouseState == GLFW_PRESS && previousLeftMouseState == GLFW_RELEASE) {
                    gMenuMode = MENU_MODE_MAIN;
                }
                previousLeftMouseState = currentLeftMouseState;
            }
        } else {
            updateCamera(window, deltaTime);

            if(!isFreeze()) {
                updateField();  
            }

            glUseProgram(Render.shader.program);
            updateMat();
            updateTexture();
            renderGrid();
            renderTexture();
        }

        glfwSwapBuffers(window);             
    }

    cleanupMenuResources();
    cleanupRenderResources();
    freeField();
    glDeleteProgram(Render.shader.program);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
