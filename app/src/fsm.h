#pragma once
#include "app_types.h"
#include "state_handlers/state_handlers.h"
#include <zephyr/kernel.h>

int fsm_init(struct VCUState *state);
int fsm_update_state(struct VCUState *state, enum FSMTransitions transition);
int fsm_commit_state(struct VCUState *state);

enum FSMTransitions fsm_do_state(struct VCUState *state, struct VCUPorts *ports);
