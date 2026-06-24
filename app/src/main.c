#include "udp_transport.h"
#include "fsm_thread.h"
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define RECV_BUF_SIZE 1500

#define DHCP_TIMEOUT_MS    10000
#define STATIC_IP_ADDR     "192.168.8.100"
#define STATIC_IP_NETMASK  "255.255.255.0"

static void log_timer_handler(struct k_timer *timer) {
  LOG_DBG("Hello, World from Zephyr!");
}

K_TIMER_DEFINE(log_timer, log_timer_handler, NULL);

struct UDPTransport udp_transport;

// DHCP Handler Setup

static struct net_mgmt_event_callback dhcp_cb;
static atomic_t dhcp_bound = ATOMIC_INIT(0);
static struct k_work_delayable dhcp_timeout_work;

static void start_transport(struct net_if *iface) {
  udp_transport_init(&udp_transport);
  fsm_thread_start(&udp_transport);
}

static void dhcp_fallback_handler(struct k_work *work) {
  if (atomic_get(&dhcp_bound)) {
    return;
  }

  LOG_WRN("DHCP timeout, falling back to static IP: %s", STATIC_IP_ADDR);

  struct net_if *iface = net_if_get_default();
  net_dhcpv4_stop(iface);

  struct in_addr addr, netmask;
  net_addr_pton(AF_INET, STATIC_IP_ADDR, &addr);
  net_addr_pton(AF_INET, STATIC_IP_NETMASK, &netmask);

  net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
  net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask);

  start_transport(iface);
}

static void dhcp_handler(struct net_mgmt_event_callback *cb,
                         uint64_t mgmt_event, struct net_if *iface) {
  if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
    atomic_set(&dhcp_bound, 1);
    k_work_cancel_delayable(&dhcp_timeout_work);

    char buf[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &iface->config.dhcpv4.requested_ip, buf,
                  sizeof(buf));

    LOG_INF("Received IP Lease: %s", buf);
    start_transport(iface);
  }
}

int main(void) {

  // get dhcp lease first
  net_mgmt_init_event_callback(&dhcp_cb, dhcp_handler,
                               NET_EVENT_IPV4_DHCP_BOUND);
  net_mgmt_add_event_callback(&dhcp_cb);

  struct net_if *iface = net_if_get_default();
  net_dhcpv4_start(iface);

  k_work_init_delayable(&dhcp_timeout_work, dhcp_fallback_handler);
  k_work_schedule(&dhcp_timeout_work, K_MSEC(DHCP_TIMEOUT_MS));

  k_timer_start(&log_timer, K_MSEC(1000), K_MSEC(1000));
  return 0;
}
