#ifndef UTILS_H
#define UTILS_H

typedef struct Projection {
    float mat[16];
} Projection;

void makeOrtho(float left, float right, float bottom, float top, float near, float far, Projection* projection);

void invertOrtho(Projection* proj, Projection* i);
Projection* getProjection(void);

struct grid{
    float top, bot;
    float right, left;
    GLuint VAO, VBO;
    int count;
};

struct quad{
    float vert[16];
    unsigned int ind[6];
    GLuint VAO, VBO, EBO;
};

struct shader{
    GLint program;
    GLint mask;                
    GLint projection;                
    GLint model;                
    GLint view;                
    GLint mode;
};

struct render_state {
    struct quad quad;
    struct grid grid;
    struct shader shader;
    GLuint maskTex;
    unsigned char* textureData;
    int textureWidth;
    int textureHeight;
    GLuint menuVAO;
    GLuint menuVBO;
    GLuint menuEBO;
    GLuint menuStartTex;
    GLuint menuExitTex;
    GLuint menuFontTex;
    GLuint menuTemplTex;
    GLuint menuTemp1Tex;
    GLuint menuTemp2Tex;
    GLuint menuTemp3Tex;
    GLuint menuTemp4Tex;
    GLuint menuAboutTex;
    GLuint menuFaqTex;
    int menuReady;
    int menuFontReady;
};

extern struct render_state Render;

char* readShader(const char* path);
void gridInit();
void quadInit();
void programCreate();
void updateMat();
void updateTexture();
void renderGrid();
void renderTexture();
int menuInit();
void renderMenu(const char* sizeX, const char* sizeY, int inputMode, int activeField);
int menuHitTestNdc(float x, float y, int inputMode);
void cleanupMenuResources();
void cleanupRenderResources();
void renderTemplatesMenu(void);
int templatesHitTestNdc(float x, float y);
void renderAboutMenu(void);
void renderFaqMenu(void);

#endif