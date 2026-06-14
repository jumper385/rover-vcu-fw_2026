#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

static void log_timer_handler(struct k_timer *timer)
{
  LOG_INF("Hello, World from Zephyr!");
}

K_TIMER_DEFINE(log_timer, log_timer_handler, NULL);

int main(void) {
  k_timer_start(&log_timer, K_MSEC(1000), K_MSEC(1000));
  return 0;
}
