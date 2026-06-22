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

  k_msgq_init(&udp_transport->rx_msgq, (char *)udp_transport->rx_msgq_buffer,
              sizeof(struct UDPReceivedPacket), 100);

  k_msgq_init(&udp_transport->tx_msgq, (char *)udp_transport->tx_msgq_buffer,
              sizeof(struct UDPTransmittedPacket), 100);

  k_thread_create(&udp_rx_thread_data, udp_rx_stack,
                  K_THREAD_STACK_SIZEOF(udp_rx_stack), udp_rx_thread,
                  udp_transport, NULL, NULL, UDP_TRANSPORT_THREAD_PRIORITY, 0,
                  K_NO_WAIT);

  LOG_INF("Socket ready...");

  return 0;
}

struct UDPReceivedPacket
udp_transport_rx_parse(struct UDPTransport *udp_transport, uint8_t *buf,
                       ssize_t len) {
  struct UDPReceivedPacket pkt = {0};
  struct UDPReceivedPacket empty_pkt = {0};

  // confirm pkt is sized right...
  ssize_t min_pkt_len = sizeof(struct UDPPacketHeader);
  ssize_t max_pkt_len = min_pkt_len + sizeof(struct UDPReceivedPacket);

  if (len > max_pkt_len) {
    LOG_WRN("Received Packet too long (%zd bytes). Dropping...", len);
    return pkt;
  }

  if (len < min_pkt_len) {
    LOG_WRN("Received Packet too small (%zd bytes). Dropping...", len);
    return pkt;
  }

  // get pkt type;
  struct UDPPacketHeader header;
  memcpy(&header, buf, sizeof(header));

  // start parsing packet
  uint8_t *payload = buf + sizeof(header);
  pkt.type = (enum PacketType)header.type;
  ssize_t payload_len = len - sizeof(header);

  switch (pkt.type) {

  case PKT_RX_DRIVE_POS_CMD:
    if (payload_len < (ssize_t)sizeof(pkt.drive_vel_cmd))
      goto too_short;
    memcpy(&pkt.drive_vel_cmd, payload, sizeof(pkt.drive_vel_cmd));
    break;

  case PKT_RX_DRIVE_VEL_CMD:
    if (payload_len < (ssize_t)sizeof(pkt.drive_pos_cmd))
      goto too_short;
    memcpy(&pkt.drive_pos_cmd, payload, sizeof(pkt.drive_pos_cmd));
    break;

  case PKT_RX_ARM_CMD:
    if (payload_len < (ssize_t)sizeof(pkt.arm_cmd))
      goto too_short;
    memcpy(&pkt.arm_cmd, payload, sizeof(pkt.arm_cmd));
    break;

  case PKT_RX_SW_SAFETY_CMD:
    if (payload_len < (ssize_t)sizeof(pkt.sw_safety_cmd))
      goto too_short;
    memcpy(&pkt.sw_safety_cmd, payload, sizeof(pkt.sw_safety_cmd));
    break;

  default:
    LOG_WRN("Unknown Packet Type 0x%02x, Dropping...", pkt.type);
  }

  return pkt;

too_short:
  LOG_WRN("Payload too short for type %d (%zd bytes), dropping...", pkt.type,
          payload_len);
  return empty_pkt;
}

void udp_rx_thread(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  struct UDPTransport *udp_transport = (struct UDPTransport *)p1;
  // int sock = udp_transport->udp_sock;

  uint8_t rx_buf[UDP_PACKET_SIZE];

  LOG_INF("UDP RX Thread started, listenign on port %d", UDP_PORT);

  for (;;) {

    // wait for new msg
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

    int null_loc = pkt_len > UDP_PACKET_SIZE
                       ? UDP_PACKET_SIZE
                       : pkt_len; // truncate if > UDP_PACKET_SIZE

    rx_buf[null_loc - 1] = '\0'; // null char termiate before printout!!! (heh)

    uint8_t rx_ip_buf[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &src_addr.sin_addr, rx_ip_buf, NET_IPV4_ADDR_LEN);

    LOG_DBG("Received %zd bytes from %s", pkt_len, rx_ip_buf);
    LOG_DBG("TEXT: %s", rx_buf);

    // when new pkt rx process it; then enqueue if valid
    struct UDPReceivedPacket rxd_pkt =
        udp_transport_rx_parse(udp_transport, rx_buf, pkt_len);

    int ret = k_msgq_put(&udp_transport->rx_msgq, &rxd_pkt, K_NO_WAIT);
    if (ret < 0) {
      LOG_WRN("rx_msgq full, dropping packet %d", rxd_pkt.type);
    }
  }
}
