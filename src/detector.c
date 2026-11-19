#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "life.h"
#include "detector.h"

#define MAX_CLUSTERS_PER_GEN 512

typedef struct {
    uint64_t hash;
    int refX, refY;
    int cellCount;
    int boxW, boxH;
} ClusterRecord;

typedef struct {
    ClusterRecord clusters[MAX_CLUSTERS_PER_GEN];
    int count;
} HistoryFrame;

typedef struct {
    int x, y;
    int ux, uy;
} BFSNode;

static DetectorAlgorithm s_currentAlgo = DETECTOR_ALGO_TOPOLOGICAL;
static GlobalCycleResult s_globalResult;

static HistoryFrame s_history[MAX_HISTORY_STEPS];
static int s_historyHead = 0;
static int s_totalGenerations = 0;
static DetectionSummary s_lastSummary;

static int *s_visited = NULL;
static int s_visitedToken = 0;
static BFSNode *s_bfsQueue = NULL;
static int *s_clusterCells = NULL;
static int *s_clusterUx = NULL;
static int *s_clusterUy = NULL;
static int s_capacity = 0;

// Буферы константной памяти O(1) для алгоритмов Флойда и Брента
static unsigned char *s_buf1 = NULL;
static unsigned char *s_buf2 = NULL;
static unsigned char *s_buf3 = NULL;
static unsigned char *s_buf4 = NULL;

// Перемешивающая 64-битная хеш-функция (SplitMix64)
static inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// Сброс истории поколений и результатов глобального поиска
void detectorReset(void){
    s_historyHead = 0;
    s_totalGenerations = 0;
    memset(s_history, 0, sizeof(s_history));
    memset(&s_lastSummary, 0, sizeof(s_lastSummary));
    memset(&s_globalResult, 0, sizeof(s_globalResult));
}

// Инициализация памяти детектора под размеры поля
void detectorInit(int width, int height){
    if (s_visited) {
        detectorFree();
    }
    s_capacity = width * height;
    s_visited = (int*)calloc(s_capacity, sizeof(int));
    s_bfsQueue = (BFSNode*)malloc(s_capacity * sizeof(BFSNode));
    s_clusterCells = (int*)malloc(s_capacity * sizeof(int));
    s_clusterUx = (int*)malloc(s_capacity * sizeof(int));
    s_clusterUy = (int*)malloc(s_capacity * sizeof(int));

    s_buf1 = (unsigned char*)malloc(s_capacity * sizeof(unsigned char));
    s_buf2 = (unsigned char*)malloc(s_capacity * sizeof(unsigned char));
    s_buf3 = (unsigned char*)malloc(s_capacity * sizeof(unsigned char));
    s_buf4 = (unsigned char*)malloc(s_capacity * sizeof(unsigned char));

    s_visitedToken = 1;
    detectorReset();
}

// Освобождение памяти детектора
void detectorFree(void){
    if (s_visited) { free(s_visited); s_visited = NULL; }
    if (s_bfsQueue) { free(s_bfsQueue); s_bfsQueue = NULL; }
    if (s_clusterCells) { free(s_clusterCells); s_clusterCells = NULL; }
    if (s_clusterUx) { free(s_clusterUx); s_clusterUx = NULL; }
    if (s_clusterUy) { free(s_clusterUy); s_clusterUy = NULL; }

    if (s_buf1) { free(s_buf1); s_buf1 = NULL; }
    if (s_buf2) { free(s_buf2); s_buf2 = NULL; }
    if (s_buf3) { free(s_buf3); s_buf3 = NULL; }
    if (s_buf4) { free(s_buf4); s_buf4 = NULL; }

    s_visitedToken = 0;
    s_capacity = 0;
}

// Установка активного алгоритма анализа
void detectorSetAlgorithm(DetectorAlgorithm algo){
    s_currentAlgo = algo;
    memset(&s_globalResult, 0, sizeof(s_globalResult));
}

// Получение текущего алгоритма анализа
DetectorAlgorithm detectorGetAlgorithm(void){
    return s_currentAlgo;
}

// Циклическое переключение алгоритма (Topological -> Floyd -> Brent)
void detectorCycleAlgorithm(void){
    s_currentAlgo = (DetectorAlgorithm)((s_currentAlgo + 1) % 3);
    memset(&s_globalResult, 0, sizeof(s_globalResult));
    if (s_currentAlgo != DETECTOR_ALGO_TOPOLOGICAL) {
        detectorAnalyze(NULL);
    }
}

// Название алгоритма для отображения в интерфейсе
const char* detectorGetAlgorithmName(DetectorAlgorithm algo){
    switch (algo) {
        case DETECTOR_ALGO_TOPOLOGICAL: return "Topological R=2 Hash";
        case DETECTOR_ALGO_FLOYD:       return "Floyd (Tortoise & Hare O(1))";
        case DETECTOR_ALGO_BRENT:       return "Brent (Teleportation O(1))";
        default:                        return "Unknown";
    }
}

// Изолированный шаг симуляции Конвея по байтовому буферу (без изменения Life.state)
static void simulateStep(const unsigned char *src, unsigned char *dst, int w, int h){
    for (int y = 0; y < h; y++) {
        int ym = (y == 0) ? (h - 1) : (y - 1);
        int yp = (y == h - 1) ? 0 : (y + 1);
        const unsigned char *rowM = src + ym * w;
        const unsigned char *row0 = src + y * w;
        const unsigned char *rowP = src + yp * w;
        unsigned char *outRow = dst + y * w;

        for (int x = 0; x < w; x++) {
            int xm = (x == 0) ? (w - 1) : (x - 1);
            int xp = (x == w - 1) ? 0 : (x + 1);

            int neighbors = rowM[xm] + rowM[x] + rowM[xp] +
                            row0[xm] +           row0[xp] +
                            rowP[xm] + rowP[x] + rowP[xp];
            unsigned char live = row0[x];
            outRow[x] = (neighbors == 3 || (live && neighbors == 2)) ? 1 : 0;
        }
    }
}

// Тороидальное сравнение двух состояний поля (учитывает как стационарное совпадение, так и сдвиг корабля)
static int checkBoardsEqual(const unsigned char *a, const unsigned char *b, int w, int h, int *outDx, int *outDy){
    int total = w * h;
    if (memcmp(a, b, (size_t)total) == 0) {
        if (outDx) *outDx = 0;
        if (outDy) *outDy = 0;
        return 1;
    }

    // Быстрая проверка числа живых клеток и сбор до 4 опорных точек для фильтра сдвига
    int countA = 0, countB = 0;
    int sampleX[4] = {0}, sampleY[4] = {0};
    int sampleCount = 0;

    for (int y = 0; y < h; y++) {
        const unsigned char *ra = a + y * w;
        const unsigned char *rb = b + y * w;
        for (int x = 0; x < w; x++) {
            if (ra[x]) {
                if (sampleCount < 4) {
                    sampleX[sampleCount] = x;
                    sampleY[sampleCount] = y;
                    sampleCount++;
                }
                countA++;
            }
            if (rb[x]) countB++;
        }
    }

    if (countA != countB || countA == 0) {
        return 0;
    }

    // Перебор кандидатов тороидального сдвига по совпадению первой опорной клетки
    int ax0 = sampleX[0];
    int ay0 = sampleY[0];

    for (int by = 0; by < h; by++) {
        const unsigned char *rb = b + by * w;
        for (int bx = 0; bx < w; bx++) {
            if (!rb[bx]) continue;

            int rawDx = (bx - ax0 + w) % w;
            int rawDy = (by - ay0 + h) % h;
            if (rawDx == 0 && rawDy == 0) continue;

            // Быстрый отсев по оставшимся опорным точкам
            int quickOk = 1;
            for (int s = 1; s < sampleCount; s++) {
                int tx = (sampleX[s] + rawDx) % w;
                int ty = (sampleY[s] + rawDy) % h;
                if (!b[ty * w + tx]) {
                    quickOk = 0;
                    break;
                }
            }
            if (!quickOk) continue;

            // Полная побайтовая проверка всех живых клеток поля при данном сдвиге
            int fullMatch = 1;
            for (int y = 0; y < h && fullMatch; y++) {
                int ty = y + rawDy;
                if (ty >= h) ty -= h;
                const unsigned char *ra = a + y * w;
                const unsigned char *rbt = b + ty * w;
                for (int x = 0; x < w; x++) {
                    if (ra[x]) {
                        int tx = x + rawDx;
                        if (tx >= w) tx -= w;
                        if (!rbt[tx]) {
                            fullMatch = 0;
                            break;
                        }
                    }
                }
            }

            if (fullMatch) {
                int dx = rawDx;
                int dy = rawDy;
                if (dx > w / 2) dx -= w;
                if (dy > h / 2) dy -= h;
                if (outDx) *outDx = dx;
                if (outDy) *outDy = dy;
                return 1;
            }
        }
    }

    return 0;
}

// Глобальный поиск цикла алгоритмом Флойда ("Черепаха и заяц", O(1) памяти)
GlobalCycleResult detectorRunFloyd(int maxSteps){
    GlobalCycleResult res = {0};
    res.algo = DETECTOR_ALGO_FLOYD;
    res.algoName = "Floyd (Tortoise & Hare O(1))";

    if (!Life.state || Life.width == 0 || Life.height == 0 || !s_buf1) return res;

    int w = (int)Life.width;
    int h = (int)Life.height;
    int total = w * h;

    unsigned char *s0 = s_buf1;
    unsigned char *tortoise = s_buf2;
    unsigned char *hare = s_buf3;
    unsigned char *temp = s_buf4;

    int cellCount = 0;
    for (int i = 0; i < total; i++) {
        s0[i] = Life.state[i].cur ? 1 : 0;
        cellCount += s0[i];
    }
    res.cellCount = cellCount;
    if (cellCount == 0) return res;

    // Фаза 1: черепаха делает 1 шаг, заяц делает 2 шага до встречи в цикле
    simulateStep(s0, tortoise, w, h);
    simulateStep(tortoise, hare, w, h);

    int steps = 1;
    int detected = 0;
    int dx = 0, dy = 0;

    while (steps <= maxSteps) {
        if (checkBoardsEqual(tortoise, hare, w, h, &dx, &dy)) {
            detected = 1;
            break;
        }
        simulateStep(tortoise, temp, w, h);
        memcpy(tortoise, temp, (size_t)total);

        simulateStep(hare, temp, w, h);
        simulateStep(temp, hare, w, h);
        steps++;
    }

    if (!detected) {
        res.stepCount = maxSteps;
        return res;
    }

    // Фаза 2: определение предпериода mu (длины переходного процесса до входа в цикл)
    memcpy(tortoise, s0, (size_t)total);
    int mu = 0;
    int muDx = 0, muDy = 0;
    while (!checkBoardsEqual(tortoise, hare, w, h, &muDx, &muDy) && mu <= maxSteps) {
        simulateStep(tortoise, temp, w, h);
        memcpy(tortoise, temp, (size_t)total);

        simulateStep(hare, temp, w, h);
        memcpy(hare, temp, (size_t)total);
        mu++;
    }

    // Фаза 3: нахождение минимального фундаментального периода P от точки входа в цикл
    int period = 1;
    simulateStep(tortoise, hare, w, h);
    while (!checkBoardsEqual(tortoise, hare, w, h, &dx, &dy) && period <= maxSteps) {
        simulateStep(hare, temp, w, h);
        memcpy(hare, temp, (size_t)total);
        period++;
    }

    res.detected = 1;
    res.period = period;
    res.transient = mu;
    res.stepCount = steps;
    res.dx = dx;
    res.dy = dy;
    return res;
}

// Глобальный поиск цикла алгоритмом Брента (телепортация степенями двойки, O(1) памяти)
GlobalCycleResult detectorRunBrent(int maxSteps){
    GlobalCycleResult res = {0};
    res.algo = DETECTOR_ALGO_BRENT;
    res.algoName = "Brent (Teleportation O(1))";

    if (!Life.state || Life.width == 0 || Life.height == 0 || !s_buf1) return res;

    int w = (int)Life.width;
    int h = (int)Life.height;
    int total = w * h;

    unsigned char *s0 = s_buf1;
    unsigned char *teleport = s_buf2;
    unsigned char *runner = s_buf3;
    unsigned char *temp = s_buf4;

    int cellCount = 0;
    for (int i = 0; i < total; i++) {
        s0[i] = Life.state[i].cur ? 1 : 0;
        cellCount += s0[i];
    }
    res.cellCount = cellCount;
    if (cellCount == 0) return res;

    // Фаза 1: экспоненциальное расширение окна сравнения 2^k с телепортацией опорного состояния
    memcpy(teleport, s0, (size_t)total);
    simulateStep(s0, runner, w, h);

    int power = 1;
    int lam = 1;
    int steps = 1;
    int detected = 0;
    int dx = 0, dy = 0;

    while (steps <= maxSteps) {
        if (checkBoardsEqual(runner, teleport, w, h, &dx, &dy)) {
            detected = 1;
            break;
        }
        if (power == lam) {
            memcpy(teleport, runner, (size_t)total);
            power *= 2;
            lam = 0;
        }
        simulateStep(runner, temp, w, h);
        memcpy(runner, temp, (size_t)total);
        lam++;
        steps++;
    }

    if (!detected) {
        res.stepCount = maxSteps;
        return res;
    }

    int period = lam;

    // Фаза 2: определение предпериода mu сдвигом второго указателя на найденный период P
    memcpy(teleport, s0, (size_t)total);
    memcpy(runner, s0, (size_t)total);
    for (int i = 0; i < period; i++) {
        simulateStep(runner, temp, w, h);
        memcpy(runner, temp, (size_t)total);
    }

    int mu = 0;
    int muDx = 0, muDy = 0;
    while (!checkBoardsEqual(teleport, runner, w, h, &muDx, &muDy) && mu <= maxSteps) {
        simulateStep(teleport, temp, w, h);
        memcpy(teleport, temp, (size_t)total);

        simulateStep(runner, temp, w, h);
        memcpy(runner, temp, (size_t)total);
        mu++;
    }

    res.detected = 1;
    res.period = period;
    res.transient = mu;
    res.stepCount = steps;
    res.dx = dx;
    res.dy = dy;
    return res;
}

// Перенос результата глобального детектора (Флойд/Брент) на разметку клеток для рендера
static void applyGlobalResultToCells(const GlobalCycleResult *gr, DetectionSummary *outSummary){
    memset(&s_lastSummary, 0, sizeof(DetectionSummary));
    int totalCells = (int)(Life.width * Life.height);

    for (int i = 0; i < totalCells; i++) {
        Life.state[i].type = CELL_TYPE_NORMAL;
        Life.state[i].cycleCount = 0;
    }

    if (gr->detected) {
        unsigned short cellType = (gr->dx != 0 || gr->dy != 0) ? CELL_TYPE_SPACESHIP :
                                  (gr->period == 1) ? CELL_TYPE_STILL_LIFE : CELL_TYPE_OSCILLATOR;
        PatternType pType = (gr->dx != 0 || gr->dy != 0) ? PATTERN_SPACESHIP :
                            (gr->period == 1) ? PATTERN_STILL_LIFE : PATTERN_OSCILLATOR;

        for (int i = 0; i < totalCells; i++) {
            if (Life.state[i].cur) {
                Life.state[i].type = cellType;
                Life.state[i].cycleCount = 31;
            }
        }

        s_lastSummary.totalObjects = 1;
        s_lastSummary.patterns[0] = (DetectedPattern){
            .type = pType,
            .period = gr->period,
            .dx = gr->dx,
            .dy = gr->dy,
            .cellCount = gr->cellCount,
            .boxW = (int)Life.width,
            .boxH = (int)Life.height,
            .refX = 0,
            .refY = 0
        };
        if (pType == PATTERN_STILL_LIFE) s_lastSummary.stillLifeCount = 1;
        else if (pType == PATTERN_OSCILLATOR) s_lastSummary.oscillatorCount = 1;
        else if (pType == PATTERN_SPACESHIP) s_lastSummary.spaceshipCount = 1;
    }

    if (outSummary) {
        memcpy(outSummary, &s_lastSummary, sizeof(DetectionSummary));
    }
}

// Построение инвариантного к сдвигу хеша по развернутым локальным координатам
static uint64_t calculateClusterHash(int cellCount, int minUx, int minUy, int boxW, int boxH) {
    uint64_t hash = 0;
    for (int i = 0; i < cellCount; i++) {
        uint32_t lx = (uint32_t)(s_clusterUx[i] - minUx);
        uint32_t ly = (uint32_t)(s_clusterUy[i] - minUy);
        uint64_t pt = ((uint64_t)lx << 32) | (uint64_t)ly;
        hash ^= splitmix64(pt);
    }
    hash ^= splitmix64(((uint64_t)(uint32_t)boxW << 32) | (uint64_t)(uint32_t)boxH);
    hash ^= splitmix64((uint64_t)cellCount);
    return hash;
}

// Топологический анализ поля: R=2 BFS-кластеризация и классификация по истории
static void runTopologicalAnalysis(DetectionSummary *outSummary){
    if (!Life.state || Life.width == 0 || Life.height == 0 || !s_visited || !s_bfsQueue) return;

    memset(&s_lastSummary, 0, sizeof(DetectionSummary));

    // Быстрый токенный сброс посещенных клеток без лишнего memset
    s_visitedToken++;
    if (s_visitedToken == 0) {
        memset(s_visited, 0, s_capacity * sizeof(int));
        s_visitedToken = 1;
    }

    int w = (int)Life.width;
    int h = (int)Life.height;
    int totalCells = w * h;

    // Сброс прошлой разметки перед новым шагом анализа
    for (int i = 0; i < totalCells; i++) {
        Life.state[i].type = CELL_TYPE_NORMAL;
        Life.state[i].cycleCount = 0;
    }

    HistoryFrame *currentFrame = &s_history[s_historyHead];
    currentFrame->count = 0;

    int maxLookback = (s_totalGenerations < MAX_HISTORY_STEPS - 1)
                    ? s_totalGenerations
                    : (MAX_HISTORY_STEPS - 1);

    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            int startIdx = sy * w + sx;
            if (!Life.state[startIdx].cur || s_visited[startIdx] == s_visitedToken) {
                continue;
            }

            // Поиск связного кластера в радиусе взаимодействия R=2 с разворотом тора
            int head = 0, tail = 0;
            int cellCount = 0;
            int minUx = sx, maxUx = sx;
            int minUy = sy, maxUy = sy;

            s_bfsQueue[tail++] = (BFSNode){sx, sy, sx, sy};
            s_visited[startIdx] = s_visitedToken;

            while (head < tail) {
                BFSNode curr = s_bfsQueue[head++];
                s_clusterCells[cellCount] = curr.y * w + curr.x;
                s_clusterUx[cellCount] = curr.ux;
                s_clusterUy[cellCount] = curr.uy;
                cellCount++;

                if (curr.ux < minUx) minUx = curr.ux;
                if (curr.ux > maxUx) maxUx = curr.ux;
                if (curr.uy < minUy) minUy = curr.uy;
                if (curr.uy > maxUy) maxUy = curr.uy;

                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = (curr.x + dx + w) % w;
                        int ny = (curr.y + dy + h) % h;
                        int nIdx = ny * w + nx;
                        if (Life.state[nIdx].cur && s_visited[nIdx] != s_visitedToken) {
                            s_visited[nIdx] = s_visitedToken;
                            s_bfsQueue[tail++] = (BFSNode){nx, ny, curr.ux + dx, curr.uy + dy};
                        }
                    }
                }
            }

            int boxW = maxUx - minUx + 1;
            int boxH = maxUy - minUy + 1;
            int refX = ((minUx % w) + w) % w;
            int refY = ((minUy % h) + h) % h;
            uint64_t hash = calculateClusterHash(cellCount, minUx, minUy, boxW, boxH);

            // Поиск минимального фундаментального периода T = 1..maxLookback по возрастанию
            PatternType detectedKind = PATTERN_UNKNOWN;
            int detectedPeriod = 0;
            int detectedDx = 0, detectedDy = 0;

            for (int step = 1; step <= maxLookback; step++) {
                int pastIdx = (s_historyHead - step + MAX_HISTORY_STEPS) % MAX_HISTORY_STEPS;
                HistoryFrame *pastFrame = &s_history[pastIdx];

                // 1) Проверка на неподвижную фигуру или осциллятор на шаге T
                for (int p = 0; p < pastFrame->count; p++) {
                    ClusterRecord *past = &pastFrame->clusters[p];
                    if (past->hash == hash && past->cellCount == cellCount &&
                        past->boxW == boxW && past->boxH == boxH &&
                        past->refX == refX && past->refY == refY) {
                        detectedKind = (step == 1) ? PATTERN_STILL_LIFE : PATTERN_OSCILLATOR;
                        detectedPeriod = step;
                        detectedDx = 0;
                        detectedDy = 0;
                        break;
                    }
                }
                if (detectedKind != PATTERN_UNKNOWN) break;

                // 2) Проверка на смещение космического корабля с учетом пределов скорости Конвея (c/2, c/4)
                for (int p = 0; p < pastFrame->count; p++) {
                    ClusterRecord *past = &pastFrame->clusters[p];
                    if (past->hash == hash && past->cellCount == cellCount &&
                        past->boxW == boxW && past->boxH == boxH) {
                        int dx = refX - past->refX;
                        if (dx > w / 2) dx -= w;
                        if (dx < -w / 2) dx += w;

                        int dy = refY - past->refY;
                        if (dy > h / 2) dy -= h;
                        if (dy < -h / 2) dy += h;

                        int isSpaceshipMotion = 0;
                        if (dx == 0 && abs(dy) > 0 && abs(dy) * 2 <= step) {
                            isSpaceshipMotion = 1;
                        } else if (dy == 0 && abs(dx) > 0 && abs(dx) * 2 <= step) {
                            isSpaceshipMotion = 1;
                        } else if (abs(dx) > 0 && abs(dx) == abs(dy) && abs(dx) * 4 <= step) {
                            isSpaceshipMotion = 1;
                        }

                        if (isSpaceshipMotion) {
                            detectedKind = PATTERN_SPACESHIP;
                            detectedPeriod = step;
                            detectedDx = dx;
                            detectedDy = dy;
                            break;
                        }
                    }
                }
                if (detectedKind != PATTERN_UNKNOWN) break;
            }

            // Разметка клеток кластера для визуализации в рендере
            if (detectedKind != PATTERN_UNKNOWN) {
                unsigned short cellType = CELL_TYPE_NORMAL;
                if (detectedKind == PATTERN_SPACESHIP) {
                    cellType = CELL_TYPE_SPACESHIP;
                } else if (detectedKind == PATTERN_OSCILLATOR) {
                    cellType = CELL_TYPE_OSCILLATOR;
                } else if (detectedKind == PATTERN_STILL_LIFE) {
                    cellType = CELL_TYPE_STILL_LIFE;
                }

                for (int i = 0; i < cellCount; i++) {
                    int cIdx = s_clusterCells[i];
                    Life.state[cIdx].type = cellType;
                    Life.state[cIdx].cycleCount = 31;
                }

                if (s_lastSummary.totalObjects < MAX_OBJECTS_TRACKED) {
                    int idx = s_lastSummary.totalObjects++;
                    s_lastSummary.patterns[idx] = (DetectedPattern){
                        .type = detectedKind,
                        .period = detectedPeriod,
                        .dx = detectedDx,
                        .dy = detectedDy,
                        .cellCount = cellCount,
                        .boxW = boxW,
                        .boxH = boxH,
                        .refX = refX,
                        .refY = refY
                    };
                    if (detectedKind == PATTERN_STILL_LIFE) s_lastSummary.stillLifeCount++;
                    else if (detectedKind == PATTERN_OSCILLATOR) s_lastSummary.oscillatorCount++;
                    else if (detectedKind == PATTERN_SPACESHIP) s_lastSummary.spaceshipCount++;
                }
            }

            // Сохранение кластера в текущий кадр истории
            if (currentFrame->count < MAX_CLUSTERS_PER_GEN && cellCount <= 2048) {
                currentFrame->clusters[currentFrame->count++] = (ClusterRecord){
                    .hash = hash,
                    .refX = refX,
                    .refY = refY,
                    .cellCount = cellCount,
                    .boxW = boxW,
                    .boxH = boxH
                };
            }
        }
    }

    if (outSummary) {
        memcpy(outSummary, &s_lastSummary, sizeof(DetectionSummary));
    }

    s_historyHead = (s_historyHead + 1) % MAX_HISTORY_STEPS;
    s_totalGenerations++;
}

// Главный диспетчер анализа: выбор между топологическим методом, Флойдом и Брентом
void detectorAnalyze(DetectionSummary *outSummary){
    if (!Life.state || Life.width == 0 || Life.height == 0 || !s_visited) return;

    if (s_currentAlgo == DETECTOR_ALGO_TOPOLOGICAL) {
        runTopologicalAnalysis(outSummary);
    } else if (s_currentAlgo == DETECTOR_ALGO_FLOYD) {
        s_globalResult = detectorRunFloyd(600);
        applyGlobalResultToCells(&s_globalResult, outSummary);
    } else if (s_currentAlgo == DETECTOR_ALGO_BRENT) {
        s_globalResult = detectorRunBrent(600);
        applyGlobalResultToCells(&s_globalResult, outSummary);
    }
}

// Возврат последней сводки распознанных структур
const DetectionSummary* detectorGetLastSummary(void){
    return &s_lastSummary;
}

// Возврат результата последнего глобального поиска цикла (Флойд / Брент)
const GlobalCycleResult* detectorGetGlobalResult(void){
    return &s_globalResult;
}
