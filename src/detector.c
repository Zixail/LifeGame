#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "life.h"
#include "detector.h"

#define MAX_CLUSTERS_PER_GEN 512
#define MAX_HISTORY_STEPS 256

typedef struct {
    uint64_t hash;
    int refX, refY;
    int cellCount;
} ClusterRecord;

typedef struct {
    ClusterRecord clusters[MAX_CLUSTERS_PER_GEN];
    int count;
} HistoryFrame;

typedef struct {
    int x, y;
} BFSNode;

static HistoryFrame s_history[MAX_HISTORY_STEPS];
static int s_historyHead = 0;
static int *s_visited = NULL;
static int s_capacity = 0;
static BFSNode *s_bfsQueue = NULL;
static int *s_clusterCells = NULL;

void detectorInit(int width, int height){
    s_capacity = width * height;
    s_visited = (int*)calloc(s_capacity, sizeof(int));
    s_bfsQueue = (BFSNode*)malloc(s_capacity * sizeof(BFSNode));
    s_clusterCells = (int*)malloc(s_capacity * sizeof(int));
}

void detectorFree(void){
    if (s_visited) free(s_visited);
    if (s_bfsQueue) free(s_bfsQueue);
    if (s_clusterCells) free(s_clusterCells);
    s_visited = NULL;
    s_bfsQueue = NULL;
    s_clusterCells = NULL;
}

static inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static uint64_t calculateHash(int *cells, int count) {
    uint64_t hash = 0;
    int minX = 1000, minY = 1000;
    
    for (int i = 0; i < count; i++) {
        int x = cells[i] % Life.width;
        int y = cells[i] / Life.width;
        if (x < minX) minX = x;
        if (y < minY) minY = y;
    }
    
    for (int i = 0; i < count; i++) {
        int x = (cells[i] % Life.width) - minX;
        int y = (cells[i] / Life.width) - minY;
        hash ^= splitmix64((uint64_t)(x * Life.width + y));
    }
    return hash;
}

void detectorAnalyze(DetectionSummary *outSummary){
    if (!s_visited || !s_bfsQueue || !s_clusterCells) return;
    
    memset(s_visited, 0, s_capacity * sizeof(int));
    
    HistoryFrame *currentFrame = &s_history[s_historyHead];
    currentFrame->count = 0;
    int objects = 0;

    for (int i = 0; i < s_capacity; i++) {
        if (Life.state[i].cur && !s_visited[i]) {
            int head = 0, tail = 0;
            s_bfsQueue[tail++] = (BFSNode){i % Life.width, i / Life.width};
            s_visited[i] = 1;
            int clusterSize = 0;

            while (head < tail) {
                BFSNode curr = s_bfsQueue[head++];
                s_clusterCells[clusterSize++] = curr.y * Life.width + curr.x;

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = (curr.x + dx + Life.width) % Life.width;
                        int ny = (curr.y + dy + Life.height) % Life.height;
                        int nIdx = ny * Life.width + nx;
                        if (Life.state[nIdx].cur && !s_visited[nIdx]) {
                            s_visited[nIdx] = 1;
                            s_bfsQueue[tail++] = (BFSNode){nx, ny};
                        }
                    }
                }
            }
            
            uint64_t hash = calculateHash(s_clusterCells, clusterSize);
            
            if (currentFrame->count < MAX_CLUSTERS_PER_GEN) {
                currentFrame->clusters[currentFrame->count++] = (ClusterRecord){
                    hash, 0, 0, clusterSize
                };
            }
            objects++;
        }
    }
    
    s_historyHead = (s_historyHead + 1) % MAX_HISTORY_STEPS;
    
    if (outSummary) {
        memset(outSummary, 0, sizeof(DetectionSummary));
        outSummary->totalObjects = objects;
    }
}
