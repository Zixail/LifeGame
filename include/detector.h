#ifndef DETECTOR_H
#define DETECTOR_H

#include <stdint.h>
#include "life.h"

#define MAX_HISTORY_STEPS 256
#define MAX_OBJECTS_TRACKED 64

// Режимы работы детектора циклов
typedef enum {
    DETECTOR_ALGO_TOPOLOGICAL = 0, // Покластерный топологический анализ (BFS R=2)
    DETECTOR_ALGO_FLOYD = 1,       // Алгоритм Флойда "Черепаха и заяц" O(1) RAM
    DETECTOR_ALGO_BRENT = 2        // Алгоритм Брента (телепортация степенями двойки) O(1) RAM
} DetectorAlgorithm;

// Типы распознанных циклических структур
typedef enum {
    PATTERN_UNKNOWN = 0,
    PATTERN_STILL_LIFE = 1,
    PATTERN_OSCILLATOR = 2,
    PATTERN_SPACESHIP = 3
} PatternType;

// Информация об одном распознанном объекте (кластере)
typedef struct {
    PatternType type;
    int period;
    int dx, dy;
    int cellCount;
    int boxW, boxH;
    int refX, refY;
} DetectedPattern;

// Сводка анализа всего поля за один шаг симуляции
typedef struct {
    int totalObjects;
    int stillLifeCount;
    int oscillatorCount;
    int spaceshipCount;
    DetectedPattern patterns[MAX_OBJECTS_TRACKED];
} DetectionSummary;

// Результат глобального поиска цикла (для алгоритмов Флойда и Брента)
typedef struct {
    int detected;
    int period;
    int transient;   // Предпериод mu (время стабилизации до входа в цикл)
    int stepCount;   // Затраченное число шагов эволюции поля
    int dx, dy;
    int cellCount;
    DetectorAlgorithm algo;
    const char *algoName;
} GlobalCycleResult;

// Инициализация и освобождение буферов детектора
void detectorInit(int width, int height);
void detectorFree(void);
void detectorReset(void);

// Управление выбором активного алгоритма детекции
void detectorSetAlgorithm(DetectorAlgorithm algo);
DetectorAlgorithm detectorGetAlgorithm(void);
void detectorCycleAlgorithm(void);
const char* detectorGetAlgorithmName(DetectorAlgorithm algo);

// Запуск анализа поля активным алгоритмом и получение результатов
void detectorAnalyze(DetectionSummary *outSummary);
const DetectionSummary* detectorGetLastSummary(void);
const GlobalCycleResult* detectorGetGlobalResult(void);

// Независимые функции глобального поиска циклов (O(1) памяти)
GlobalCycleResult detectorRunFloyd(int maxSteps);
GlobalCycleResult detectorRunBrent(int maxSteps);

#endif
