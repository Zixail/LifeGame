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

// Перемешивающая 64-битная хеш-функция (SplitMix64)
static inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// Сброс истории поколений и последней сводки
void detectorReset(void){
    s_historyHead = 0;
    s_totalGenerations = 0;
    memset(s_history, 0, sizeof(s_history));
    memset(&s_lastSummary, 0, sizeof(s_lastSummary));
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

    s_visitedToken = 0;
    s_capacity = 0;
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
void detectorAnalyze(DetectionSummary *outSummary){
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

// Возврат последней сводки распознанных структур
const DetectionSummary* detectorGetLastSummary(void){
    return &s_lastSummary;
}
