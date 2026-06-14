#include "ports.h"

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

int ports_read_inputs(struct VCUPorts *ports) {
  // udp packets used to do setpoints for VCU or control the FSM from upstream
#ifdef CONFIG_VCU_PORTS_MOCK
#else
#endif
  return 0;
}

int ports_write_outputs(struct VCUPorts *ports) {
  // packets stream telemetry upstream  to control computer (basestation)
#ifdef CONFIG_VCU_PORTS_MOCK
#else
#endif
  return 0;
}
