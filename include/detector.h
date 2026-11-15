#ifndef DETECTOR_H
#define DETECTOR_H

#include <stdint.h>
#include "life.h"

typedef enum {
    PATTERN_UNKNOWN = 0,
    PATTERN_STILL_LIFE = 1,
    PATTERN_OSCILLATOR = 2,
    PATTERN_SPACESHIP = 3
} PatternType;

typedef struct {
    PatternType type;
    int period;
    int dx, dy;
    int cellCount;
    int boxW, boxH;
    int refX, refY;
} DetectedPattern;

typedef struct {
    int totalObjects;
    int stillLifeCount;
    int oscillatorCount;
    int spaceshipCount;
    DetectedPattern patterns[32];
} DetectionSummary;

void detectorInit(int width, int height);
void detectorFree(void);
void detectorAnalyze(DetectionSummary *outSummary);

#endif
