#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hello_world);

static void hello_timer_handler(struct k_timer *timer)
{
	LOG_INF("Hello, World!");
}

K_TIMER_DEFINE(hello_timer, hello_timer_handler, NULL);

int main(void)
{
	k_timer_start(&hello_timer, K_SECONDS(1), K_SECONDS(1));
	return 0;
}
