#include "udp_transport.h"
#include "fsm_thread.h"
#include "dhcp_req.h"
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

static void log_timer_handler(struct k_timer *timer) {
  LOG_DBG("Hello, World from Zephyr!");
}

K_TIMER_DEFINE(log_timer, log_timer_handler, NULL);

struct UDPTransport udp_transport;

static void on_network_ready(struct net_if *iface) {
  udp_transport_init(&udp_transport);
  fsm_thread_start(&udp_transport);
}

int main(void) {

  dhcp_req_start(on_network_ready);

  k_timer_start(&log_timer, K_MSEC(1000), K_MSEC(1000));
  return 0;
}
