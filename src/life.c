#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "life.h"

struct life_state Life;

int countNeighbors(int x0, int y0){
    int count = 0 - Life.state[y0 * Life.width + x0].cur;
    for(int dx = -1; dx <= 1; ++dx){
        for(int dy = -1; dy <= 1; ++dy){
            int x = (x0 + Life.width + dx) % Life.width;
            int y = (y0 + Life.height + dy) % Life.height;
            count += Life.state[y * Life.width + x].cur;
        }
    }
    return count;
}

void updateField(){
    int total = Life.width * Life.height;

    for (int i = 0; i < total; i++) {
        Life.state[i].two = Life.state[i].prev;
        Life.state[i].prev = Life.state[i].cur;
    }

    for(int y = 0; y < Life.height; ++y){
        for(int x = 0; x < Life.width; ++x){
            int count = countNeighbors(x, y);
            int live = Life.state[y * Life.width + x].cur;
            Life.state[y * Life.width + x].next = ((count > (2 - live)) && (count < 4));
        }
    }

    for (int i = 0; i < total; i++) {
        if (Life.state[i].next == Life.state[i].cur || Life.state[i].next == Life.state[i].two) {
            if (Life.state[i].cycleCount < 31) Life.state[i].cycleCount++;
        }
        else {
            Life.state[i].cycleCount = 0;
        }
    }

    for (int i = 0; i < total; i++) {
        Life.state[i].cur = Life.state[i].next;
    }
}


void loadPattern(int pattern) {
    for (int i = 0; i < Life.width * Life.height; i++) {
        Life.state[i].cur = 0;
    }
    int cx = Life.width / 2;
    int cy = Life.height / 2;
    switch (pattern) {
        case 1: // Глайдер
            Life.state[cy * Life.width + cx].cur = 1;
            Life.state[(cy+1) * Life.width + cx+1].cur = 1;
            Life.state[(cy+2) * Life.width + cx-1].cur = 1;
            Life.state[(cy+2) * Life.width + cx].cur = 1;
            Life.state[(cy+2) * Life.width + cx+1].cur = 1;
            break;
        case 2: // Квадрат 2х2
            Life.state[cy * Life.width + cx].cur = 1;
            Life.state[cy * Life.width + cx+1].cur = 1;
            Life.state[(cy+1) * Life.width + cx].cur = 1;
            Life.state[(cy+1) * Life.width + cx+1].cur = 1;
            break;
        case 3: // Мигалка
            Life.state[cy * Life.width + cx-1].cur = 1;
            Life.state[cy * Life.width + cx].cur = 1;
            Life.state[cy * Life.width + cx+1].cur = 1;
            break;
        case 4: // Пустое поле
            break;
        default:
            return;
    }

    for (int i = 0; i < Life.width * Life.height; i++) {
        Life.state[i].cycleCount = 0;
        Life.state[i].prev = 0;
        Life.state[i].two = 0;
    } 
}

void initField(int width, int height){
    Life.width = width;
    Life.height = height;
    Life.state = (State*)calloc(Life.width * Life.height, sizeof(State));
}

void freeField(){
    free(Life.state);
    Life.state = NULL;
    Life.width = 0;
    Life.height = 0;
}