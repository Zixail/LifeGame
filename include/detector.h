#ifndef DETECTOR_H
#define DETECTOR_H

#include <stdint.h>
#include "life.h"

#define MAX_HISTORY_STEPS 256
#define MAX_OBJECTS_TRACKED 64

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

// Инициализация буферов детектора под размер поля
void detectorInit(int width, int height);

// Освобождение памяти детектора
void detectorFree(void);

// Сброс накопленной истории поколений
void detectorReset(void);

// Пошаговый топологический анализ поля
void detectorAnalyze(DetectionSummary *outSummary);

// Получение последней сводки распознавания
const DetectionSummary* detectorGetLastSummary(void);

#endif
