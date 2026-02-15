#include <Arduino_RouterBridge.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "system_display.h"

static const size_t kJsonBufferSize = 128;
static const size_t kCmdBufferSize = 64;
static const size_t kResponseBufferSize = 64;
static const size_t kHelpBufferSize = 512;
static const uint32_t kDefaultDisplayPeriodMs = 200;
static const uint8_t kDefaultBrightness = 7;
static const uint32_t kSetupDelayMs = 2000;

typedef struct {
  String latest_json;
  volatile bool has_new_data;
} RpcState;

typedef enum {
  DISPLAY_CPU = 0,
  DISPLAY_MEMORY = 1,
  DISPLAY_DISK = 2
} DisplayMode;

typedef struct {
  DisplayMode mode;
  uint32_t update_period_ms;
  uint32_t last_draw_ms;
  uint32_t last_sample_ms;
  uint8_t brightness;
} DisplayConfig;

typedef struct {
  const char* name;
  const char* help_line;
} Command;

static RpcState g_rpc_state = {String(), false};
static DisplayConfig g_display_config = {DISPLAY_CPU, kDefaultDisplayPeriodMs, 0, 0, kDefaultBrightness};
static char g_cmd_buffer[kCmdBufferSize];
static size_t g_cmd_len = 0;
static SystemStats g_last_stats = {0.0f, 0.0f, 0.0f};

static const Command kCommands[] = {
  {"metric", "metric cpu|mem|disk     - Display metric"},
  {"period", "period <200-5000>       - Update interval (ms)"},
  {"bright", "bright <0-7>            - LED brightness"},
  {"show",   "show                    - Show config"},
  {"help",   "help                    - Show this help"}
};
static const size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

static void rpc_state_set_json(const String& json_data);
static bool fetch_latest_json(char* buffer, size_t buffer_size);
static bool parse_float_field(const char* json, const char* key, float* value_out);
static bool update_stats_from_json(const char* json, SystemStats* stats);
static void print_monitor_help(void);
static int find_command(const char* cmd);
static const char* get_command_arg(const char* cmd, int cmd_index);
static void handle_metric_command(const char* arg);
static void handle_period_command(const char* arg);
static void handle_bright_command(const char* arg);
static void handle_show_command(void);
static void handle_help_command(void);
static void handle_command(const char* cmd);
static void process_monitor_input(void);

/**
 * Stores JSON data in global RPC state and sets new data flag.
 *
 * @param json_data JSON string from MPU
 */
static void rpc_state_set_json(const String& json_data) {
  g_rpc_state.latest_json = json_data;
  g_rpc_state.has_new_data = true;
}

/**
 * RPC handler to receive system statistics from MPU.
 *
 * @param json_data JSON formatted system statistics
 *
 * @return Response string "OK"
 *
 * @remarks
 * This handler is called from high-priority RPC thread.
 * Only performs minimal work (set flag and store data).
 * Do not use Monitor.print() here to avoid deadlock.
 */
String receive_system_stats(String json_data) {
  rpc_state_set_json(json_data);
  return String("OK");
}

/**
 * Retrieves the latest JSON from RPC state if new data is available.
 *
 * @param buffer Output buffer for JSON string
 * @param buffer_size Size of output buffer
 *
 * @return true if new data was retrieved; false otherwise
 *
 * @remarks
 * Protected by noInterrupts/interrupts to prevent RPC callback interference.
 */
static bool fetch_latest_json(char* buffer, size_t buffer_size) {
  bool has_update = false;

  if (!buffer || buffer_size == 0) return false;

  noInterrupts();
  if (g_rpc_state.has_new_data) {
    has_update = true;
    g_rpc_state.latest_json.toCharArray(buffer, buffer_size);
    g_rpc_state.has_new_data = false;
  }
  interrupts();

  return has_update;
}

/**
 * Extracts a floating-point field from a JSON string.
 *
 * @param json JSON string to parse
 * @param key Field name to search for (e.g., "\"cpu\"")
 * @param value_out Output pointer for the extracted value
 *
 * @return true if parse successful; false otherwise
 *
 * @remarks
 * Handles integer and fractional parts separately for precision.
 * Supports negative numbers.
 */
static bool parse_float_field(const char* json, const char* key, float* value_out) {
  const char* pos;
  bool negative = false;
  uint32_t int_part = 0;
  uint32_t frac_part = 0;
  uint32_t frac_div = 1;
  bool has_digit = false;
  float value;

  if (!value_out || !json || !key) return false;

  pos = strstr(json, key);
  if (!pos) return false;

  pos = strchr(pos, ':');
  if (!pos) return false;

  pos++;
  while (*pos && isspace(static_cast<unsigned char>(*pos))) {
    pos++;
  }

  if (*pos == '-') {
    negative = true;
    pos++;
  }

  while (*pos && isdigit(static_cast<unsigned char>(*pos))) {
    has_digit = true;
    int_part = (int_part * 10u) + (uint32_t)(*pos - '0');
    pos++;
  }

  if (*pos == '.') {
    pos++;
    while (*pos && isdigit(static_cast<unsigned char>(*pos))) {
      has_digit = true;
      frac_part = (frac_part * 10u) + (uint32_t)(*pos - '0');
      frac_div *= 10u;
      pos++;
    }
  }

  if (!has_digit) return false;

  value = (float)int_part + ((float)frac_part / (float)frac_div);
  if (negative) value = -value;

  *value_out = value;
  return true;
}

/**
 * Converts JSON formatted system statistics to SystemStats structure.
 *
 * @param json JSON string containing system metrics
 * @param stats Output structure for parsed data
 *
 * @return true if ALL fields were parsed successfully; false otherwise
 *
 * @remarks
 * All-or-nothing update. If any field fails to parse, stats is unchanged.
 */
static bool update_stats_from_json(const char* json, SystemStats* stats) {
  float cpu = 0.0f;
  float memory = 0.0f;
  float disk = 0.0f;

  if (!json || !stats) return false;

  if (!parse_float_field(json, "\"cpu\"", &cpu)) return false;
  if (!parse_float_field(json, "\"memory\"", &memory)) return false;
  if (!parse_float_field(json, "\"disk\"", &disk)) return false;

  stats->cpu = cpu;
  stats->memory = memory;
  stats->disk = disk;
  return true;
}

/**
 * Displays available commands to the Monitor.
 *
 * @remarks
 * Generates output dynamically from kCommands[] table.
 */
static void print_monitor_help(void) {
  char buffer[kHelpBufferSize];
  size_t offset = 0;

  offset += snprintf(buffer + offset, sizeof(buffer) - offset, "===== COMMANDS =====\n");

  for (size_t i = 0; i < kCommandCount; i++) {
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s\n", kCommands[i].help_line);
  }

  offset += snprintf(buffer + offset, sizeof(buffer) - offset, "====================");

  Monitor.println(buffer);
  Monitor.flush();
}

/**
 * Searches for a command by name in the command table.
 *
 * @param cmd Command string (e.g., "metric cpu")
 *
 * @return Table index (0-4), or -1 if not found
 *
 * @remarks
 * Requires exact command name match, not partial.
 */
static int find_command(const char* cmd) {
  const char* name = nullptr;
  const char* cmd_ptr = nullptr;

  while (*cmd && isspace(static_cast<unsigned char>(*cmd))) {
    cmd++;
  }
  if (!*cmd) return -1;

  for (size_t i = 0; i < kCommandCount; i++) {
    name = kCommands[i].name;
    cmd_ptr = cmd;

    while (*name && *cmd_ptr && *name == *cmd_ptr) {
      name++;
      cmd_ptr++;
    }

    if (*name == '\0' && (*cmd_ptr == '\0' || isspace(static_cast<unsigned char>(*cmd_ptr)))) {
      return (int)i;
    }
  }
  return -1;
}

/**
 * Extracts argument portion from a command string.
 *
 * @param cmd Command string (e.g., "metric cpu")
 * @param cmd_index Index from find_command()
 *
 * @return Pointer to argument string, or empty string if none
 *
 * @remarks
 * Uses cmd_index to determine command name length.
 */
static const char* get_command_arg(const char* cmd, int cmd_index) {
  const char* ptr = cmd;

  while (*ptr && isspace(static_cast<unsigned char>(*ptr))) {
    ptr++;
  }

  ptr += strlen(kCommands[cmd_index].name);

  while (*ptr && isspace(static_cast<unsigned char>(*ptr))) {
    ptr++;
  }

  return ptr;
}

/**
 * Handles "metric" command to select display metric.
 *
 * @param arg Metric name ("cpu", "mem", "disk")
 *
 * @remarks
 * Outputs error message if argument is invalid.
 */
static void handle_metric_command(const char* arg) {
  const char* mode_name = nullptr;
  char buffer[kResponseBufferSize];

  if (strncmp(arg, "cpu", 3) == 0) {
    g_display_config.mode = DISPLAY_CPU;
    mode_name = "cpu";
  } else if (strncmp(arg, "mem", 3) == 0 || strncmp(arg, "memory", 6) == 0) {
    g_display_config.mode = DISPLAY_MEMORY;
    mode_name = "memory";
  } else if (strncmp(arg, "disk", 4) == 0) {
    g_display_config.mode = DISPLAY_DISK;
    mode_name = "disk";
  } else {
    Monitor.println("[ERR] metric cpu|mem|disk");
    return;
  }

  snprintf(buffer, sizeof(buffer), "[OK] metric set to %s", mode_name);
  Monitor.println(buffer);
  Monitor.flush();
}

/**
 * Handles "period" command to set update interval.
 *
 * @param arg Update period in milliseconds
 *
 * @remarks
 * Auto-clamps to [200, 5000]. Outputs actual value applied.
 */
static void handle_period_command(const char* arg) {
  uint32_t value = 0;
  bool has_digit = false;
  char buffer[kResponseBufferSize];

  while (*arg && isdigit(static_cast<unsigned char>(*arg))) {
    has_digit = true;
    value = (value * 10u) + (uint32_t)(*arg - '0');
    arg++;
  }

  if (!has_digit) {
    Monitor.println("[ERR] period <200-5000>");
    return;
  }

  if (value < 200u)  value = 200u;
  if (value > 5000u) value = 5000u;

  g_display_config.update_period_ms = value;

  snprintf(buffer, sizeof(buffer), "[OK] period set to %lu ms", (unsigned long)value);
  Monitor.println(buffer);
  Monitor.flush();
}

/**
 * Handles "bright" command to set LED brightness.
 *
 * @param arg Brightness level (0-7)
 *
 * @remarks
 * Auto-clamps to maximum value kMaxShade. Applies immediately to hardware.
 */
static void handle_bright_command(const char* arg) {
  uint32_t value = 0;
  bool has_digit = false;
  char buffer[kResponseBufferSize];

  while (*arg && isdigit(static_cast<unsigned char>(*arg))) {
    has_digit = true;
    value = (value * 10u) + (uint32_t)(*arg - '0');
    arg++;
  }

  if (!has_digit) {
    Monitor.println("[ERR] bright <0-7>");
    return;
  }

  if (value > kMaxShade) value = kMaxShade;

  g_display_config.brightness = (uint8_t)value;
  system_display_set_brightness(g_display_config.brightness);

  snprintf(buffer, sizeof(buffer), "[OK] brightness set to %u", g_display_config.brightness);
  Monitor.println(buffer);
  Monitor.flush();
}

/**
 * Handles "show" command to display current configuration.
 *
 * @remarks
 * Outputs format: [mode period_ms brightness]
 */
static void handle_show_command(void) {
  const char* mode_str = "cpu";
  char buffer[kResponseBufferSize];

  if (g_display_config.mode == DISPLAY_MEMORY) {
    mode_str = "mem";
  } else if (g_display_config.mode == DISPLAY_DISK) {
    mode_str = "disk";
  }

  snprintf(buffer, sizeof(buffer), "[%s %lums b%u]",
           mode_str, (unsigned long)g_display_config.update_period_ms, g_display_config.brightness);
  Monitor.println(buffer);
  Monitor.flush();
}

/**
 * Handles "help" command to show available commands.
 */
static void handle_help_command(void) {
  print_monitor_help();
}

/**
 * Main command dispatcher.
 *
 * @param cmd Command string input from user
 *
 * @remarks
 * Dispatches to appropriate handler based on command table lookup.
 * Table-driven design for maintainability.
 */
static void handle_command(const char* cmd) {
  int cmd_index = 0;
  const char* arg = nullptr;

  if (!cmd) return;

  cmd_index = find_command(cmd);
  if (cmd_index < 0) {
    Monitor.println("[ERR] unknown cmd (type help)");
    return;
  }

  arg = get_command_arg(cmd, cmd_index);

  if (cmd_index == 0) {
    handle_metric_command(arg);
  } else if (cmd_index == 1) {
    handle_period_command(arg);
  } else if (cmd_index == 2) {
    handle_bright_command(arg);
  } else if (cmd_index == 3) {
    handle_show_command();
  } else if (cmd_index == 4) {
    handle_help_command();
  }
}

/**
 * Processes input from Monitor serial interface.
 *
 * @remarks
 * Reads characters until newline or carriage return detected.
 * Each completed line is passed to handle_command().
 * Buffer overflow protection included.
 */
static void process_monitor_input(void) {
  int ch = 0;
  char c = '\0';

  while (Monitor.available() > 0) {
    ch = Monitor.read();
    if (ch < 0) return;

    c = (char)ch;

    if (c == '\n' || c == '\r') {
      if (g_cmd_len > 0) {
        g_cmd_buffer[g_cmd_len] = '\0';
        handle_command(g_cmd_buffer);
        g_cmd_len = 0;
      }
      continue;
    }

    if (g_cmd_len + 1 < kCmdBufferSize) {
      g_cmd_buffer[g_cmd_len++] = c;
    } else {
      g_cmd_len = 0;
      Monitor.println("[ERR] cmd too long");
    }
  }
}

/**
 * Arduino setup function - called once at startup.
 *
 * @remarks
 * Initializes Bridge, Monitor, and display system.
 * Registers RPC handler for system statistics.
 */
void setup() {
  Bridge.begin();
  Monitor.begin();
  delay(kSetupDelayMs);

  Bridge.provide("receive_system_stats", receive_system_stats);

  system_display_init();
  system_display_set_brightness(g_display_config.brightness);

  Monitor.println("[OK] Setup complete - type 'help' for commands");
  Monitor.flush();
}

/**
 * Arduino main loop - executes repeatedly.
 *
 * @remarks
 * Implements two asynchronous tasks:
 * 1. Data Reception Task: Updates ring buffer immediately when new RPC data arrives
 * 2. Display Task: Renders LED matrix at fixed intervals (period)
 * 
 * This architecture ensures:
 * - High-frequency sampling driven by Python's transmission rate
 * - Fixed-period display updates independent of Python's timer precision
 * - No cumulative timing drift in long-term operation
 */
void loop() {
  char json_buffer[kJsonBufferSize];
  uint32_t now = 0;
  MetricType metric = METRIC_CPU;
  uint8_t heights[kMatrixWidth];
  bool stats_updated = false;
  float push_value = 0.0f;

  process_monitor_input();

  if (g_display_config.mode == DISPLAY_MEMORY) metric = METRIC_MEMORY;
  else if (g_display_config.mode == DISPLAY_DISK) metric = METRIC_DISK;

  if (fetch_latest_json(json_buffer, sizeof(json_buffer))) {
    if (update_stats_from_json(json_buffer, &g_last_stats)) {
      stats_updated = true;
    }
  }

  now = millis();
  if (stats_updated && (now - g_display_config.last_sample_ms >= g_display_config.update_period_ms)) {
    g_display_config.last_sample_ms = now;

    push_value = g_last_stats.cpu;
    if (metric == METRIC_MEMORY) push_value = g_last_stats.memory;
    else if (metric == METRIC_DISK) push_value = g_last_stats.disk;

    system_display_push_sample(metric, push_value);
  }

  if (now - g_display_config.last_draw_ms >= g_display_config.update_period_ms) {
    g_display_config.last_draw_ms = now;
    buffer_to_heights(&g_display_state.metrics[metric], heights);
    draw_bar_graph_on_matrix(heights);
  }
}
