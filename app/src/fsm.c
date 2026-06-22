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
    execute_handler(state, ports);
    break;
  case FSM_STOPPING:
    stopped_handler(state, ports);
    break;
  case FSM_IDLE:
    idle_handler(state, ports);
    break;
  case FSM_EXECUTE:
    execute_handler(state, ports);
    break;
  case FSM_HOLDING:
    holding_handler(state, ports);
    break;
  case FSM_HELD:
    held_handler(state, ports);
    break;
  case FSM_SUSPENDING:
    suspending_handler(state, ports);
    break;
  case FSM_SUSPENDED:
    suspended_handler(state, ports);
    break;
  case FSM_ABORTING:
    aborting_handler(state, ports);
    break;
  case FSM_ABORTED:
    aborted_handler(state, ports);
    break;
  case FSM_CLEARING:
    clearing_handler(state, ports);
    break;

  default:
    break;
  }

  // really shouldn't be here...
  return TRAN_EVT_ABORT;
}

int fsm_commit_state(struct VCUState *state) {
  state->cs = state->ns;
  return 0;
}
