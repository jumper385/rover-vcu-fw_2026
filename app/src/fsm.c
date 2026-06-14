#include "fsm.h"

int fsm_init(struct VCUState *state) {
  state->cs = FSM_BOOTING;
  state->ns = FSM_BOOTING;
  return 0;
}

int update_state(struct VCUState *state, enum FSMTransitions transition) {
  state->ns = state->cs;

  switch (state->cs) {
  case FSM_BOOTING:
    if (transition == TRAN_EVT_BOOTED)
      state->ns = FSM_STOPPED;
    break;

  case FSM_STOPPED:
    if (transition == TRAN_EVT_ENABLE)
      state->ns = FSM_IDLE;
    break;

  case FSM_IDLE:
    if (transition == TRAN_EVT_RUN)
      state->ns = FSM_EXECUTE;
    break;

  case FSM_EXECUTE:
    if (transition == TRAN_EVT_STOP)
      state->ns = FSM_STOPPING;
    else if (transition == TRAN_EVT_HOLD)
      state->ns = FSM_HOLDING;
    else if (transition == TRAN_EVT_SUSPEND)
      state->ns = FSM_SUSPENDING;
    break;

  case FSM_STOPPING:
    if (transition == TRAN_EVT_STOP)
      state->ns = FSM_STOPPED;
    break;

  case FSM_HOLDING:
    if (transition == TRAN_EVT_HOLD)
      state->ns = FSM_HELD;
    break;

  case FSM_HELD:
    if (transition == TRAN_EVT_RESUME)
      state->ns = FSM_EXECUTE;
    break;

  case FSM_SUSPENDING:
    if (transition == TRAN_EVT_SUSPEND)
      state->ns = FSM_SUSPENDED;
    break;

  case FSM_SUSPENDED:
    if (transition == TRAN_EVT_RESUME)
      state->ns = FSM_EXECUTE;
    break;

  default:
    state->ns = FSM_ABORTED;
    break;
  }

  return 0;
}
