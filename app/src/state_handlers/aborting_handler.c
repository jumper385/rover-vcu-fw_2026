#include "state_handlers.h"

enum FSMTransitions aborting_handler(struct VCUState *state, struct VCUPorts *ports)
{
  return TRAN_EVT_NONE;
}
