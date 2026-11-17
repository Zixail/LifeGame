#ifndef LIFE_H
#define LIFE_H

// Типы клеточных структур для визуальной классификации
typedef enum {
    CELL_TYPE_NORMAL = 0,
    CELL_TYPE_STILL_LIFE = 1,
    CELL_TYPE_OSCILLATOR = 2,
    CELL_TYPE_SPACESHIP = 3
} CellType;

typedef struct {
    unsigned short cur : 1;
    unsigned short next : 1;
    unsigned short prev : 1;
    unsigned short two : 1;
    unsigned short type : 2;        // 0: обычная, 1: натюрморт, 2: осциллятор, 3: корабль
    unsigned short cycleCount : 5;  // 0..31
    unsigned short reserved : 5;
} State;

struct life_state {
    State *state;
    unsigned int width, height;
};

// Идентификаторы встроенных шаблонов (1..4 сохранены для совместимости с меню шаблонов)
enum {
    PATTERN_GLIDER = 1,
    PATTERN_SQUARE = 2,
    PATTERN_BLINKER = 3,
    PATTERN_EMPTY = 4,
    PATTERN_FIGURE_EIGHT = 5,
    PATTERN_PULSAR = 6,
    PATTERN_OCTAGON2 = 7,
    PATTERN_PENTADECATHLON = 8,
    PATTERN_GLIDER_GUN = 9,
    PATTERN_LWSS = 10,
    PATTERN_KOK_GALAXY = 11,
    PATTERN_MWSS = 12,
    PATTERN_HWSS = 13,
    PATTERN_CLOCK = 14,
    PATTERN_EATER1 = 15,
    PATTERN_METHUSELAHS = 16,
    PATTERN_SHOWCASE = 17,
    PATTERN_MEGA_FIELD = 18,
    PATTERN_GRAND_FIELD = 19
};

extern struct life_state Life;

int countNeighbors(int x0, int y0);

void updateField();

void loadPattern(int pattern);

const char* getPatternName(int pattern);

void initField(int width, int height);

void freeField();

void resetHistory();

#endif
