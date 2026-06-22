#include "ports.h"
#include "../udp_transport.h"
#include <string.h>

static struct UDPTransport *s_transport;

#ifdef CONFIG_VCU_PORTS_MOCK
int ports_mock_inputs(struct VCUPorts *ports, struct InputPorts in) {
  ports->in = in;
  return 0;
}

int ports_mock_outputs(struct VCUPorts *ports, struct OutputPorts out) {
  ports->out = out;
  return 0;
}
#endif

int ports_init(struct VCUPorts *ports, struct UDPTransport *transport) {
  s_transport = transport;
  memset(ports, 0, sizeof(*ports));
  return 0;
}

int ports_read_inputs(struct VCUPorts *ports) {
#ifdef CONFIG_VCU_PORTS_MOCK
  /* inputs injected externally via ports_mock_inputs */
#else
  struct UDPReceivedPacket pkt;
  while (k_msgq_get(&s_transport->rx_msgq, &pkt, K_NO_WAIT) == 0) {
    switch (pkt.type) {
    case PKT_RX_SW_SAFETY_CMD:
      ports->in.safety_cmd = pkt.sw_safety_cmd;
      ports->in.safety_cmd_pending = true;
      break;
    case PKT_RX_DRIVE_VEL_CMD:
      ports->in.drive_vel_cmd = pkt.drive_vel_cmd;
      break;
    case PKT_RX_DRIVE_POS_CMD:
      ports->in.drive_pos_cmd = pkt.drive_pos_cmd;
      break;
    default:
      break;
    }
  }
#endif
  return 0;
}

int ports_write_outputs(struct VCUPorts *ports) {
  /* TODO: serialise OutputPorts fields into UDPTransmittedPackets
   * and enqueue to s_transport->tx_msgq */
  ARG_UNUSED(ports);
  return 0;
}
