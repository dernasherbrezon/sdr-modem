#ifndef SDR_MODEM_LOG_H
#define SDR_MODEM_LOG_H

// Levels use the syslog/journald numbering, so they can be written
// directly as the "<N>" line prefix that journald understands.
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_WARN 4
#define LOG_LEVEL_INFO 6
#define LOG_LEVEL_DEBUG 7

// Messages less severe than this are dropped. Default is LOG_LEVEL_INFO.
void log_set_level(int level);

// Parses "error", "warn", "info" or "debug". Returns 0 or -1 if unknown.
int log_parse_level(const char *name, int *level);

// Adds the newline itself. error and warn go to stderr, the rest to stdout.
// When running under systemd with stdout/stderr connected to the journal
// (JOURNAL_STREAM is set), each line is prefixed with "<N>" so journalctl
// shows the proper priority. Otherwise (terminal, cli) the output is plain.
void log_write(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));

#define LOG_ERROR(...) log_write(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...) log_write(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_INFO(...) log_write(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) log_write(LOG_LEVEL_DEBUG, __VA_ARGS__)

#endif  // SDR_MODEM_LOG_H
