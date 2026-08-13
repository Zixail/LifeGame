#ifndef CALLBACKS_H
#define CALLBACKS_H
#include <GLFW/glfw3.h>

struct input_state {
	float zoom;
	float cameraX;
	float cameraY;
	double lastUpdateTime;
	double updateInterval;
	char freeze;
	char isFullscreen;
	int windowedX;
	int windowedY;
	int windowedWidth;
	int windowedHeight;
};

enum menu_mode {
    MENU_MODE_MAIN = 0,
    MENU_MODE_SIZE_INPUT = 1,
    MENU_MODE_TEMPLATES = 2,
    MENU_MODE_ABOUT = 3,
    MENU_MODE_FAQ = 4
};

extern struct input_state Input;

extern int gSelectedPattern;
extern int gMenuMode;
extern int gActiveField;
extern int gSizeConfirmRequested;
extern char gSizeX[8];
extern char gSizeY[8];

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mod);
void buffersizeCallback(GLFWwindow* window, int width, int height);
void menuKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void menuFramebufferSizeCallback(GLFWwindow* window, int width, int height);
void updateMenuTitle(GLFWwindow* window);
void menuCharCallback(GLFWwindow* window, unsigned int codepoint);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
int isFreeze();
void setCallback(GLFWwindow* window);
void setWindowImage(GLFWwindow* window);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
void processMouseClick(GLFWwindow* window, int action);
void updateCamera(GLFWwindow* window, float deltaTime);

#endif