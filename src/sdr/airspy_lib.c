#include "airspy_lib.h"
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include "../log.h"

#define AIRSPY_LIB_LOAD(name) \
  *(void **) &result->name = airspy_lib_dlsym(result->handle, #name); \
  if (result->name == NULL) { \
    airspy_lib_destroy(result); \
    return -1; \
  }

static void *airspy_lib_dlsym(void *handle, const char *symbol) {
  void *result = dlsym(handle, symbol);
  if (result == NULL) {
    LOG_ERROR("unable to load function %s: %s", symbol, dlerror());
  }
  return result;
}

static void *airspy_lib_open(void) {
  static const char *const names[] = {
#if defined(__APPLE__)
      "libairspy.dylib",
      "libairspy.0.dylib",
      "/opt/homebrew/lib/libairspy.dylib",
      "/usr/local/lib/libairspy.dylib",
#else
      "libairspy.so.0",
      "libairspy.so",
#endif
  };
  void *handle = NULL;
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) && handle == NULL; i++) {
    handle = dlopen(names[i], RTLD_LAZY);
  }
  return handle;
}

int airspy_lib_create(airspy_lib **lib) {
  struct airspy_lib_t *result = malloc(sizeof(struct airspy_lib_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct airspy_lib_t) {0};

  result->handle = airspy_lib_open();
  if (result->handle == NULL) {
    LOG_ERROR("unable to load libairspy: %s", dlerror());
    airspy_lib_destroy(result);
    return -1;
  }
  AIRSPY_LIB_LOAD(airspy_open)
  AIRSPY_LIB_LOAD(airspy_open_sn)
  AIRSPY_LIB_LOAD(airspy_close)
  AIRSPY_LIB_LOAD(airspy_get_samplerates)
  AIRSPY_LIB_LOAD(airspy_set_samplerate)
  AIRSPY_LIB_LOAD(airspy_set_sample_type)
  AIRSPY_LIB_LOAD(airspy_start_rx)
  AIRSPY_LIB_LOAD(airspy_stop_rx)
  AIRSPY_LIB_LOAD(airspy_is_streaming)
  AIRSPY_LIB_LOAD(airspy_set_freq)
  AIRSPY_LIB_LOAD(airspy_set_lna_gain)
  AIRSPY_LIB_LOAD(airspy_set_mixer_gain)
  AIRSPY_LIB_LOAD(airspy_set_vga_gain)
  AIRSPY_LIB_LOAD(airspy_set_lna_agc)
  AIRSPY_LIB_LOAD(airspy_set_mixer_agc)
  AIRSPY_LIB_LOAD(airspy_set_linearity_gain)
  AIRSPY_LIB_LOAD(airspy_set_sensitivity_gain)
  AIRSPY_LIB_LOAD(airspy_set_rf_bias)
  AIRSPY_LIB_LOAD(airspy_error_name)

  *lib = result;
  return 0;
}

void airspy_lib_destroy(airspy_lib *lib) {
  if (lib == NULL) {
    return;
  }
  if (lib->handle != NULL) {
    dlclose(lib->handle);
  }
  free(lib);
}
