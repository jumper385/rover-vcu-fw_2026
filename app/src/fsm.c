#include "fsm.h"

int fsm_init(struct VCUState *state) {
  state->cs = FSM_BOOTING;
  state->ns = FSM_BOOTING;
  return 0;
}

int fsm_update_state(struct VCUState *state, enum FSMTransitions transition) {
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

enum FSMTransitions fsm_do_state(struct VCUState *state,
                                 struct VCUPorts *ports) {
  switch (state->cs) {
  case FSM_BOOTING:
    return booting_handler(state, ports);
  case FSM_STOPPED:
    return stopped_handler(state, ports);
  case FSM_IDLE:
    return idle_handler(state, ports);
  case FSM_EXECUTE:
    return execute_handler(state, ports);
  case FSM_HOLDING:
    return holding_handler(state, ports);
  case FSM_HELD:
    return held_handler(state, ports);
  case FSM_STOPPING:
    return stopping_handler(state, ports);
  case FSM_SUSPENDING:
    return suspending_handler(state, ports);
  case FSM_SUSPENDED:
    return suspended_handler(state, ports);
  case FSM_ABORTING:
    return aborting_handler(state, ports);
  case FSM_ABORTED:
    return aborted_handler(state, ports);
  case FSM_CLEARING:
    return clearing_handler(state, ports);
  default:
    return TRAN_EVT_NONE;
  }
}

int fsm_commit_state(struct VCUState *state) {
  state->cs = state->ns;
  return 0;
}
