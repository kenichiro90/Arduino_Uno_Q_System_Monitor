#include "system_display.h"

#include <math.h>
#include <string.h>

DisplayState g_display_state;
static uint8_t g_brightness = kMaxShade;

/**
 * Clears all pixels in the frame buffer.
 *
 * @param frame Pointer to the frame buffer to clear
 */
static void clear_frame(uint8_t* frame) {
  if (!frame) return;
  memset(frame, 0, kMatrixWidth * kMatrixHeight);
}

/**
 * Sets a single pixel in the frame buffer with brightness scaling.
 *
 * @param frame Pointer to the frame buffer
 * @param x Column position (0 to kMatrixWidth-1)
 * @param y Row position (0 to kMatrixHeight-1)
 * @param shade Shade value (0 to kMaxShade), scaled by global brightness
 *
 * @remarks
 * Coordinates and shade values are validated. Shade is scaled
 * by g_brightness and rounded to the nearest integer.
 */
static void set_pixel(uint8_t* frame, uint8_t x, uint8_t y, uint8_t shade) {
  size_t index = 0;
  uint8_t clipped = 0;
  uint16_t scaled = 0;

  if (!frame) return;
  if (x >= kMatrixWidth || y >= kMatrixHeight) return;

  index = (size_t)y * kMatrixWidth + x;
  clipped = (shade > kMaxShade) ? kMaxShade : shade;
  scaled = (uint16_t)clipped * (uint16_t)g_brightness;
  scaled = (scaled + (kMaxShade / 2u)) / kMaxShade;
  frame[index] = (uint8_t)scaled;
}

/**
 * Initializes a metric buffer for time-series data storage.
 *
 * @param buffer Pointer to the buffer to initialize
 *
 * @remarks
 * The ring buffer holds up to kHistoryLen floating-point values.
 * Head pointer points to the next write position.
 */
static void metric_buffer_init(MetricBuffer* buffer) {
  if (!buffer) return;
  buffer->head = 0;
  buffer->count = 0;
  for (size_t i = 0; i < kHistoryLen; ++i) {
    buffer->values[i] = 0.0f;
  }
}

/**
 * Adds a new value to the metric ring buffer.
 *
 * @param buffer Pointer to the buffer
 * @param value New metric value to append
 *
 * @remarks
 * When the buffer is full, the oldest value is overwritten.
 * Head pointer wraps around modulo kHistoryLen.
 */
static void metric_buffer_push(MetricBuffer* buffer, float value) {
  if (!buffer) return;
  buffer->values[buffer->head] = value;
  buffer->head = (buffer->head + 1) % kHistoryLen;
  if (buffer->count < kHistoryLen) {
    buffer->count++;
  }
}

/**
 * Retrieves a historical metric value from the buffer.
 *
 * @param buffer Pointer to the buffer
 * @param recent_index Index from newest (0=latest, 1=one before, ...)
 *
 * @return Floating-point metric value, or 0.0 if buffer is empty
 *
 * @remarks
 * Indices beyond the available history return the oldest value.
 * The function safely handles the ring buffer wraparound.
 */
static float metric_buffer_get_recent(const MetricBuffer* buffer, size_t recent_index) {
  size_t last_index = 0;
  size_t index = 0;

  if (!buffer || buffer->count == 0) return 0.0f;
  if (recent_index >= buffer->count) recent_index = buffer->count - 1;

  last_index = (buffer->head + kHistoryLen - 1) % kHistoryLen;
  index = (last_index + kHistoryLen - recent_index) % kHistoryLen;
  return buffer->values[index];
}

/**
 * Converts a metric value to LED bar height in pixels.
 *
 * @param value Metric value, expected range 0.0 to 100.0 (percentage)
 *
 * @return Height in pixels (0 to kMatrixHeight)
 */
static uint8_t value_to_height(float value) {
  float normalized = 0.0f;
  float scaled = 0.0f;

  if (value <= 0.0f) return 0;
  if (value >= 100.0f) return kMatrixHeight;

  normalized = value / 100.0f;
  scaled = normalized * (float)kMatrixHeight;
  return (uint8_t)(scaled + 0.5f);
}

/**
 * Initializes the LED matrix display system with startup animation.
 *
 * @remarks
 * Each metric buffer is pre-populated with 5.0% initial values.
 * Displays a fill animation from right to left: starting with the rightmost
 * column lit, then progressively adding columns from right to left until all
 * columns are illuminated. Animation takes approximately 1.3 seconds.
 * Sets matrix_ready flag only after initialization completes.
 */
void system_display_init(void) {
  uint8_t startup_frame[kMatrixWidth * kMatrixHeight];
  uint8_t x = 0;
  uint8_t y = 0;
  uint8_t scroll_pos = 0;

  for (size_t i = 0; i < METRIC_COUNT; ++i) {
    metric_buffer_init(&g_display_state.metrics[i]);
    for (size_t j = 0; j < kMatrixWidth; ++j) {
      metric_buffer_push(&g_display_state.metrics[i], 5.0f);
    }
  }

  g_display_state.matrix_ready = false;

  g_display_state.matrix.begin();
  g_display_state.matrix.setGrayscaleBits(3);
  g_display_state.matrix.clear();
  delay(100);

  for (scroll_pos = 0; scroll_pos < kMatrixWidth; ++scroll_pos) {
    clear_frame(startup_frame);

    for (uint8_t col = 0; col <= scroll_pos; ++col) {
      x = kMatrixWidth - 1 - col;
      for (y = 0; y < kMatrixHeight; ++y) {
        set_pixel(startup_frame, x, y, kMaxShade);
      }
    }

    g_display_state.matrix.draw(startup_frame);
    delay(100);
  }

  g_display_state.matrix_ready = true;
}

/**
 * Appends a single metric value to the specified ring buffer.
 *
 * @param type Metric type (METRIC_CPU, METRIC_MEMORY, or METRIC_DISK)
 * @param value Metric value (percentage, 0.0-100.0)
 *
 * @remarks
 * Only the currently selected metric is pushed, keeping other buffers unchanged.
 */
void system_display_push_sample(MetricType type, float value) {
  if (type >= METRIC_COUNT) return;
  metric_buffer_push(&g_display_state.metrics[type], value);
}

/**
 * Renders a bar graph to the LED matrix display.
 *
 * @param heights Array of bar heights (one per column)
 *
 * @remarks
 * Bars are drawn from bottom to top. Heights beyond kMatrixHeight are clipped.
 */
void draw_bar_graph_on_matrix(const uint8_t* heights) {
  uint8_t frame[kMatrixWidth * kMatrixHeight];
  uint8_t h = 0;
  uint8_t y = 0;

  if (!heights || !g_display_state.matrix_ready) return;

  clear_frame(frame);
  for (uint8_t col = 0; col < kMatrixWidth; ++col) {
    h = heights[col];
    if (h > kMatrixHeight) h = kMatrixHeight;
    for (uint8_t row = 0; row < h; ++row) {
      y = (kMatrixHeight - 1) - row;
      set_pixel(frame, col, y, kMaxShade);
    }
  }
  g_display_state.matrix.draw(frame);
}

/**
 * Converts metric buffer data to LED bar heights.
 *
 * @param buffer Pointer to the metric ring buffer
 * @param heights Output array of heights (size kMatrixWidth)
 *
 * @remarks
 * Maps recent history from oldest to newest across columns 0 to kMatrixWidth-1.
 * Column 0 (leftmost) displays the oldest value (recent_index=12).
 * Column 12 (rightmost) displays the most recent value (recent_index=0).
 */
void buffer_to_heights(const MetricBuffer* buffer, uint8_t* heights) {
  size_t recent_index = 0;
  float v = 0.0f;

  if (!buffer || !heights) return;
  for (uint8_t col = 0; col < kMatrixWidth; ++col) {
    recent_index = kMatrixWidth - 1 - col;
    v = metric_buffer_get_recent(buffer, recent_index);
    heights[col] = value_to_height(v);
  }
}

/**
 * Sets the LED brightness level.
 *
 * @param brightness Brightness level (0 to kMaxShade)
 *
 * @remarks
 * Applied to subsequent draw operations. Clipped to maximum value.
 */
void system_display_set_brightness(uint8_t brightness) {
  if (brightness > kMaxShade) brightness = kMaxShade;
  g_brightness = brightness;
}
