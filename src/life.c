#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "life.h"
#include "detector.h"

struct life_state Life;

// Сброс истории состояний детектора
void resetHistory(){
    detectorReset();
}

// Подсчет живых соседей клетки в окрестности Мура (на торе)
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

// Шаг симуляции поля и запуск топологического анализатора
void updateField(){
    int total = (int)(Life.width * Life.height);

    for (int i = 0; i < total; i++) {
        Life.state[i].two = Life.state[i].prev;
        Life.state[i].prev = Life.state[i].cur;
    }

    for(int y = 0; y < (int)Life.height; ++y){
        for(int x = 0; x < (int)Life.width; ++x){
            int count = countNeighbors(x, y);
            int live = Life.state[y * Life.width + x].cur;
            Life.state[y * Life.width + x].next = ((count > (2 - live)) && (count < 4));
        }
    }

    for (int i = 0; i < total; i++) {
        Life.state[i].cur = Life.state[i].next;
    }

    // Анализ кластеров и обновление типов клеток для рендера
    detectorAnalyze(NULL);
}

// Размещение фигуры на поле по ASCII-схеме
static void putAscii(int ox, int oy, const char *pat){
    int x = ox, y = oy;
    for (int i = 0; pat[i] != '\0'; i++) {
        if (pat[i] == '\n') {
            y++;
            x = ox;
        } else if (pat[i] == 'X' || pat[i] == '#' || pat[i] == 'O') {
            if (y >= 0 && y < (int)Life.height && x >= 0 && x < (int)Life.width) {
                Life.state[y * Life.width + x].cur = 1;
            }
            x++;
        } else if (pat[i] == '.' || pat[i] == ' ') {
            x++;
        }
    }
}

// Возврат человекочитаемого названия выбранного шаблона
const char* getPatternName(int pattern){
    switch (pattern) {
        case PATTERN_GLIDER: return "Glider (Spaceship P4 c/4)";
        case PATTERN_SQUARE: return "Square 2x2 (Still Life P1)";
        case PATTERN_BLINKER: return "Blinker, Toad & Beacon (Oscillators P2)";
        case PATTERN_EMPTY: return "Empty Field";
        case PATTERN_FIGURE_EIGHT: return "Figure Eight (Oscillator P8)";
        case PATTERN_PULSAR: return "Pulsar (Oscillator P3)";
        case PATTERN_OCTAGON2: return "Octagon 2 (Oscillator P5)";
        case PATTERN_PENTADECATHLON: return "Pentadecathlon (Oscillator P15)";
        case PATTERN_GLIDER_GUN: return "Gosper Glider Gun (Engine P30 + Gliders)";
        case PATTERN_LWSS: return "Lightweight Spaceship LWSS (P4 c/2)";
        case PATTERN_KOK_GALAXY: return "Kok's Galaxy (Spiral Oscillator P8)";
        case PATTERN_MWSS: return "Middleweight Spaceship MWSS (P4 c/2)";
        case PATTERN_HWSS: return "Heavyweight Spaceship HWSS (P4 c/2)";
        case PATTERN_CLOCK: return "Clock (Oscillator P2)";
        case PATTERN_EATER1: return "Eater 1 & Still Lifes (P1)";
        case PATTERN_METHUSELAHS: return "Methuselahs (Acorn, Diehard, R-pentomino)";
        case PATTERN_SHOWCASE: return "SHOWCASE - All Classical Patterns";
        case PATTERN_MEGA_FIELD: return "Massive Dual-Galaxy Mega Field (184 cells, P=120)";
        case PATTERN_GRAND_FIELD: return "Grand Multi-Oscillator Field (94 cells, P=120)";
        default: return "Custom Field";
    }
}

// Загрузка витрины из 20+ классических фигур (адаптируется под размер поля)
static void loadShowcaseInternal(){
    int w = (int)Life.width;
    int h = (int)Life.height;

    if (w < 90 || h < 90) {
        // Компактная витрина для стандартной сетки (например 50x50)
        putAscii(w * 10 / 100, h * 10 / 100, "XX\nXX");
        putAscii(w * 10 / 100, h * 28 / 100, ".XX.\nX..X\n.XX.");
        putAscii(w * 10 / 100, h * 50 / 100, "XXX");
        putAscii(w * 10 / 100, h * 68 / 100, ".XXX\nXXX.");
        putAscii(w * 35 / 100, h * 12 / 100,
            "..XXX...XXX..\n"
            ".............\n"
            "X....X.X....X\n"
            "X....X.X....X\n"
            "X....X.X....X\n"
            "..XXX...XXX..\n"
            ".............\n"
            "..XXX...XXX..\n"
            "X....X.X....X\n"
            "X....X.X....X\n"
            "X....X.X....X\n"
            ".............\n"
            "..XXX...XXX..");
        putAscii(w * 38 / 100, h * 55 / 100,
            "XXXXXX.XX\n"
            "XXXXXX.XX\n"
            ".......XX\n"
            "XX.....XX\n"
            "XX.....XX\n"
            "XX.....XX\n"
            "XX.......\n"
            "XX.XXXXXX\n"
            "XX.XXXXXX");
        putAscii(w * 72 / 100, h * 20 / 100, ".X.\n..X\nXXX");
        putAscii(w * 72 / 100, h * 65 / 100, ".X..X\nX....\nX...X\nXXXX.");
        return;
    }

    int col1 = w * 6 / 100;
    int col2 = w * 22 / 100;
    int col3 = w * 44 / 100;
    int col4 = w * 75 / 100;

    // Колонка 1: Натюрморты (период 1)
    putAscii(col1, h * 5 / 100, "XX\nXX");                  // Блок
    putAscii(col1, h * 18 / 100, ".XX.\nX..X\n.XX.");       // Улей
    putAscii(col1, h * 31 / 100, ".XX.\nX..X\n.X.X\n..X."); // Каравай
    putAscii(col1, h * 44 / 100, "XX.\nX.X\n.X.");          // Лодка
    putAscii(col1, h * 57 / 100, ".X.\nX.X\n.X.");          // Лохань
    putAscii(col1, h * 70 / 100, "XX..\nX.X.\n..X.\n..XX"); // Пожиратель 1

    // Колонка 2: Базовые осцилляторы (P2 - P3)
    putAscii(col2, h * 6 / 100, "XXX");                      // Мигалка (P2)
    putAscii(col2, h * 18 / 100, ".XXX\nXXX.");              // Жаба (P2)
    putAscii(col2, h * 30 / 100, "XX..\nXX..\n..XX\n..XX");  // Маяк (P2)
    putAscii(col2, h * 44 / 100, "..X.\nX.X.\n.X.X\n.X..");  // Часы (P2)
    putAscii(col2, h * 60 / 100,                             // Пульсар (P3)
        "..XXX...XXX..\n"
        ".............\n"
        "X....X.X....X\n"
        "X....X.X....X\n"
        "X....X.X....X\n"
        "..XXX...XXX..\n"
        ".............\n"
        "..XXX...XXX..\n"
        "X....X.X....X\n"
        "X....X.X....X\n"
        "X....X.X....X\n"
        ".............\n"
        "..XXX...XXX..");

    // Колонка 3: Сложные осцилляторы (P5 - P15)
    putAscii(col3, h * 4 / 100, // Восьмиугольник 2 (P5)
        "...XX...\n"
        "..X..X..\n"
        ".X....X.\n"
        "X......X\n"
        "X......X\n"
        ".X....X.\n"
        "..X..X..\n"
        "...XX...");

    putAscii(col3, h * 23 / 100, // Восьмерка (P8)
        "XXX...\n"
        "XXX...\n"
        "XXX...\n"
        "...XXX\n"
        "...XXX\n"
        "...XXX");

    putAscii(col3, h * 39 / 100, // Галактика Кока (P8)
        "XXXXXX.XX\n"
        "XXXXXX.XX\n"
        ".......XX\n"
        "XX.....XX\n"
        "XX.....XX\n"
        "XX.....XX\n"
        "XX.......\n"
        "XX.XXXXXX\n"
        "XX.XXXXXX");

    putAscii(col3, h * 61 / 100, // Пентадекатлон (P15)
        "..X....X..\n"
        "XX.XXXX.XX\n"
        "..X....X..");

    // Колонка 4: Космические корабли (P4, c/2 и c/4)
    putAscii(col4, h * 15 / 100, // LWSS (c/2)
        ".X..X\n"
        "X....\n"
        "X...X\n"
        "XXXX.");

    putAscii(col4, h * 32 / 100, // MWSS (c/2)
        "...X..\n"
        ".X...X\n"
        "X.....\n"
        "X....X\n"
        "XXXXX.");

    putAscii(col4, h * 50 / 100, // HWSS (c/2)
        "...XX..\n"
        ".X....X\n"
        "X......\n"
        "X.....X\n"
        "XXXXXX.");

    putAscii(col4, h * 68 / 100, // Глайдер (c/4)
        ".X.\n"
        "..X\n"
        "XXX");

    // Снизу: Ружье глайдеров Госпера (P30)
    putAscii(w * 48 / 100, h * 80 / 100,
        "........................X...........\n"
        "......................X.X...........\n"
        "............XX......XX............XX\n"
        "...........X...X....XX............XX\n"
        "XX........X.....X...XX..............\n"
        "XX........X...X.XX....X.X...........\n"
        "..........X.....X.......X...........\n"
        "...........X...X....................\n"
        "............XX......................");
}

// Загрузка выбранного шаблона и сброс истории детектора
void loadPattern(int pattern){
    int total = (int)(Life.width * Life.height);
    for (int i = 0; i < total; i++) {
        Life.state[i].cur = 0;
        Life.state[i].cycleCount = 0;
        Life.state[i].prev = 0;
        Life.state[i].two = 0;
        Life.state[i].type = CELL_TYPE_NORMAL;
    }

    int w = (int)Life.width;
    int h = (int)Life.height;
    int cx = w / 2;
    int cy = h / 2;

    switch (pattern) {
        case PATTERN_GLIDER: // Глайдер (P4, c/4)
            putAscii(cx - 1, cy - 1, ".X.\n..X\nXXX");
            break;

        case PATTERN_SQUARE: // Квадрат 2х2 (P1)
            putAscii(cx - 1, cy - 1, "XX\nXX");
            break;

        case PATTERN_BLINKER: // Мигалка + Жаба + Маяк (P2)
            putAscii(cx - 10, cy, "XXX");
            putAscii(cx - 2, cy, ".XXX\nXXX.");
            putAscii(cx + 8, cy, "XX..\nXX..\n..XX\n..XX");
            break;

        case PATTERN_FIGURE_EIGHT: // Восьмерка (P8)
            putAscii(cx - 3, cy - 3,
                "XXX...\n"
                "XXX...\n"
                "XXX...\n"
                "...XXX\n"
                "...XXX\n"
                "...XXX");
            break;

        case PATTERN_PULSAR: // Пульсар (P3)
            putAscii(cx - 6, cy - 6,
                "..XXX...XXX..\n"
                ".............\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "..XXX...XXX..\n"
                ".............\n"
                "..XXX...XXX..\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                ".............\n"
                "..XXX...XXX..");
            break;

        case PATTERN_OCTAGON2: // Восьмиугольник 2 (P5)
            putAscii(cx - 4, cy - 4,
                "...XX...\n"
                "..X..X..\n"
                ".X....X.\n"
                "X......X\n"
                "X......X\n"
                ".X....X.\n"
                "..X..X..\n"
                "...XX...");
            break;

        case PATTERN_PENTADECATHLON: // Пентадекатлон (P15)
            putAscii(cx - 5, cy - 1,
                "..X....X..\n"
                "XX.XXXX.XX\n"
                "..X....X..");
            break;

        case PATTERN_GLIDER_GUN: // Ружье глайдеров Госпера (P30)
            putAscii(cx - 18, cy - 4,
                "........................X...........\n"
                "......................X.X...........\n"
                "............XX......XX............XX\n"
                "...........X...X....XX............XX\n"
                "XX........X.....X...XX..............\n"
                "XX........X...X.XX....X.X...........\n"
                "..........X.....X.......X...........\n"
                "...........X...X....................\n"
                "............XX......................");
            break;

        case PATTERN_LWSS: // Легковесный космический корабль (c/2, P4)
            putAscii(cx - 2, cy - 2,
                ".X..X\n"
                "X....\n"
                "X...X\n"
                "XXXX.");
            break;

        case PATTERN_KOK_GALAXY: // Галактика Кока (P8)
            putAscii(cx - 4, cy - 4,
                "XXXXXX.XX\n"
                "XXXXXX.XX\n"
                ".......XX\n"
                "XX.....XX\n"
                "XX.....XX\n"
                "XX.....XX\n"
                "XX.......\n"
                "XX.XXXXXX\n"
                "XX.XXXXXX");
            break;

        case PATTERN_MWSS: // Средний космический корабль (c/2, P4)
            putAscii(cx - 3, cy - 2,
                "...X..\n"
                ".X...X\n"
                "X.....\n"
                "X....X\n"
                "XXXXX.");
            break;

        case PATTERN_HWSS: // Тяжелый космический корабль (c/2, P4)
            putAscii(cx - 3, cy - 2,
                "...XX..\n"
                ".X....X\n"
                "X......\n"
                "X.....X\n"
                "XXXXXX.");
            break;

        case PATTERN_CLOCK: // Часы (P2)
            putAscii(cx - 2, cy - 2,
                "..X.\n"
                "X.X.\n"
                ".X.X\n"
                ".X..");
            break;

        case PATTERN_EATER1: // Пожиратель глайдеров + натюрморты (P1)
            putAscii(cx - 12, cy - 2, "XX..\nX.X.\n..X.\n..XX");
            putAscii(cx - 2, cy - 2, ".XX.\nX..X\n.XX.");
            putAscii(cx + 8, cy - 2, ".XX.\nX..X\n.X.X\n..X.");
            break;

        case PATTERN_METHUSELAHS: // Долгожители (Acorn, Diehard, R-пентамино)
            putAscii(cx - 22, cy, ".X.....\n...X...\nXX..XXX");
            putAscii(cx - 4, cy, "......X.\nXX......\n.X...XXX");
            putAscii(cx + 18, cy, ".XX\nXX.\n.X.");
            break;

        case PATTERN_SHOWCASE: // Полная витрина всех фигур
            loadShowcaseInternal();
            break;

        case PATTERN_MEGA_FIELD: { // Массивное поле 184 клетки (P=120)
            const char *kokAscii =
                "XXXXXX.XX\n"
                "XXXXXX.XX\n"
                ".......XX\n"
                "XX.....XX\n"
                "XX.....XX\n"
                "XX.....XX\n"
                "XX.......\n"
                "XX.XXXXXX\n"
                "XX.XXXXXX";
            putAscii(w * 8 / 100, h * 14 / 100, kokAscii);
            putAscii(w * 8 / 100, h * 64 / 100, kokAscii);
            putAscii(w * 38 / 100, h * 38 / 100,
                "..XXX...XXX..\n"
                ".............\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "..XXX...XXX..\n"
                ".............\n"
                "..XXX...XXX..\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                ".............\n"
                "..XXX...XXX..");
            putAscii(w * 72 / 100, h * 20 / 100, "..X....X..\nXX.XXXX.XX\n..X....X..");
            putAscii(w * 72 / 100, h * 50 / 100, "..X....X..\nXX.XXXX.XX\n..X....X..");
            putAscii(w * 72 / 100, h * 76 / 100,
                "...XX...\n"
                "..X..X..\n"
                ".X....X.\n"
                "X......X\n"
                "X......X\n"
                ".X....X.\n"
                "..X..X..\n"
                "...XX...");
            break;
        }

        case PATTERN_GRAND_FIELD: // Гранд-поле 94 клетки (LCM P=120)
            putAscii(w * 14 / 100, h * 14 / 100,
                "..XXX...XXX..\n"
                ".............\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "..XXX...XXX..\n"
                ".............\n"
                "..XXX...XXX..\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                "X....X.X....X\n"
                ".............\n"
                "..XXX...XXX..");
            putAscii(w * 14 / 100, h * 66 / 100,
                "...XX...\n"
                "..X..X..\n"
                ".X....X.\n"
                "X......X\n"
                "X......X\n"
                ".X....X.\n"
                "..X..X..\n"
                "...XX...");
            putAscii(w * 64 / 100, h * 16 / 100,
                "XXX...\n"
                "XXX...\n"
                "XXX...\n"
                "...XXX\n"
                "...XXX\n"
                "...XXX");
            putAscii(w * 64 / 100, h * 68 / 100,
                "..X....X..\n"
                "XX.XXXX.XX\n"
                "..X....X..");
            break;

        case 0:
        case PATTERN_EMPTY: // Пустое поле
        default:
            break;
    }

    detectorReset();
    detectorAnalyze(NULL);
}

// Инициализация игрового поля и буферов детектора
void initField(int width, int height){
    if (Life.state) {
        freeField();
    }
    Life.width = width;
    Life.height = height;
    Life.state = (State*)calloc(Life.width * Life.height, sizeof(State));
    detectorInit(width, height);
}

// Освобождение памяти игрового поля и детектора
void freeField(){
    if (Life.state) {
        free(Life.state);
        Life.state = NULL;
    }
    detectorFree();
    Life.width = 0;
    Life.height = 0;
}
