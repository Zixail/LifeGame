#ifndef LIFE_H
#define LIFE_H

typedef struct {
    unsigned char cur : 1;
    unsigned char next : 1;
    unsigned char prev : 1;
    unsigned char two : 1;
    unsigned char cycleCount : 5;
} State;

struct life_state {
    State *state;
    unsigned int width, height;
};

extern struct life_state Life;

int countNeighbors(int x0, int y0);

void updateField();

void loadPattern(int pattern);

void initField(int width, int height);

void freeField();

#endif