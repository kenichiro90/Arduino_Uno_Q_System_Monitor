#ifndef SYSTEM_DISPLAY_H
#define SYSTEM_DISPLAY_H

#include <Arduino.h>
#include <stdint.h>
#include <Arduino_LED_Matrix.h>

// --- 定数・型定義 ---
static const size_t kHistoryLen = 60;
static const uint8_t kMatrixWidth = 13;
static const uint8_t kMatrixHeight = 8;
static const uint8_t kMaxShade = 7;

typedef enum {
  METRIC_CPU = 0,
  METRIC_MEMORY = 1,
  METRIC_DISK = 2,
  METRIC_COUNT = 3
} MetricType;

typedef struct {
  float cpu;
  float memory;
  float disk;
} SystemStats;

typedef struct {
  float values[kHistoryLen];
  size_t head;
  size_t count;
} MetricBuffer;

typedef struct {
  MetricBuffer metrics[METRIC_COUNT];
  Arduino_LED_Matrix matrix;
  bool matrix_ready;
} DisplayState;

extern DisplayState g_display_state;

// --- API ---
void system_display_init(void);
void system_display_push_sample(MetricType type, float value);
void draw_bar_graph_on_matrix(const uint8_t* heights);
void buffer_to_heights(const MetricBuffer* buffer, uint8_t* heights);
void system_display_set_brightness(uint8_t brightness);

#endif
