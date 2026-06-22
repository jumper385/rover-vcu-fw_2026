#include "state_handlers.h"

enum FSMTransitions booting_handler(struct VCUState *state, struct VCUPorts *ports) {
  // send state upstream
  return TRAN_EVT_NONE;
}
