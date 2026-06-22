#pragma once
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#define UDP_PORT 5555
#define UDP_PACKET_SIZE 1024
#define UDP_THREAD_STACK_SIZE 2048
#define UDP_TRANSPORT_THREAD_PRIORITY 5

struct UDPPacketHeader {
  uint8_t type;
  uint8_t version;
  uint16_t len;
};

enum PacketType {
  PKT_DRIVE_VEL_CMD = 0x01,
  PKT_DRIVE_POS_CMD = 0x02,
  PKT_SAFETY_CMD = 0x03,
  PKT_MOTOR_TELEM = 0x10,
  PKT_SENSOR_TELEM = 0x11,
  PKT_SAFETY_STATE = 0x12
};

struct UDPTransport {
  int udp_sock;
};

int udp_transport_init(struct UDPTransport *udp_transport);
void udp_rx_thread(void *p1, void *p2, void *p3);
