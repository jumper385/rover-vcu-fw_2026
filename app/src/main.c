#include "udp_transport.h"
#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define RECV_BUF_SIZE 1500

static void log_timer_handler(struct k_timer *timer) {
  LOG_DBG("Hello, World from Zephyr!");
}

K_TIMER_DEFINE(log_timer, log_timer_handler, NULL);

struct UDPTransport udp_transport;

// DHCP Handler Setup

static struct net_mgmt_event_callback dhcp_cb;

static void dhcp_handler(struct net_mgmt_event_callback *cb,
                         uint64_t mgmt_event, struct net_if *iface) {
  if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
    char buf[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &iface->config.dhcpv4.requested_ip, buf,
                  sizeof(buf));

    LOG_INF("Received IP Lease: %s", buf);
    udp_transport_init(&udp_transport);
  }
}

int main(void) {

  int ret;

  // get dhcp lease first
  net_mgmt_init_event_callback(&dhcp_cb, dhcp_handler,
                               NET_EVENT_IPV4_DHCP_BOUND);
  net_mgmt_add_event_callback(&dhcp_cb);

  struct net_if *iface = net_if_get_default();
  net_dhcpv4_start(iface);

  // TODO: if after 10s no dhcp, set static ip; garuantees system will boot

  k_timer_start(&log_timer, K_MSEC(1000), K_MSEC(1000));
  return 0;
}
