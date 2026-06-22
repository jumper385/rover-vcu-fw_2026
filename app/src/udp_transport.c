#include "udp_transport.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(udp_transport);

K_THREAD_STACK_DEFINE(udp_rx_stack, UDP_THREAD_STACK_SIZE);
static struct k_thread udp_rx_thread_data;

int udp_transport_init(struct UDPTransport *udp_transport) {

  LOG_INF("Initializing UDP Transport Layer");

  struct sockaddr_in bind_addr;
  int ret;

  udp_transport->udp_sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udp_transport->udp_sock < 0) {
    LOG_ERR("Failed to create UDP Socket: %d", errno);
    return -1;
  }

  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(UDP_PORT);
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  ret = zsock_bind(udp_transport->udp_sock, (struct sockaddr *)&bind_addr,
                   sizeof(bind_addr));
  if (ret < 0) {
    LOG_ERR("Failed to bind UDP Socket %d", errno);
    zsock_close(udp_transport->udp_sock);
    udp_transport->udp_sock = -1;
    return -errno;
  }

  LOG_INF("Socket seems to be ready...");

  k_thread_create(&udp_rx_thread_data, udp_rx_stack,
                  K_THREAD_STACK_SIZEOF(udp_rx_stack), udp_rx_thread,
                  udp_transport, NULL, NULL, UDP_TRANSPORT_THREAD_PRIORITY, 0,
                  K_NO_WAIT);

  LOG_INF("Socket ready...");

  return 0;
}

void udp_rx_thread(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  struct UDPTransport *udp_transport = (struct UDPTransport *)p1;
  // int sock = udp_transport->udp_sock;

  uint8_t rx_buf[UDP_PACKET_SIZE];

  LOG_INF("UDP RX Thread started, listenign on port %d", UDP_PORT);

  for (;;) {

    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

    ssize_t pkt_len =
        zsock_recvfrom(udp_transport->udp_sock, rx_buf, sizeof(rx_buf) - 1, 0,
                       (struct sockaddr *)&src_addr, &addr_len);

    if (pkt_len < 0) {
      LOG_ERR("recvfrom() failed %d", errno);
      k_msleep(100); // backoff for a bit;
      continue;
    }

    rx_buf[pkt_len - 1] = '\0'; // null char termiate!!!

    uint8_t rx_ip_buf[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &src_addr.sin_addr, rx_ip_buf, NET_IPV4_ADDR_LEN);

    LOG_DBG("Received %zd bytes from %s", pkt_len, rx_ip_buf);
    LOG_DBG("TEXT: %s", rx_buf);

    // do the parsing here now...
  }
}
