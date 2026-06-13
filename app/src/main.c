#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

int main(void) {
  while (1) {
    LOG_INF("Hello, World from Zephyr!");
    k_msleep(1000);
  }
  return 1;
}
