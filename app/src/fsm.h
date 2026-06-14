#pragma once
#include "app_types.h"
#include <zephyr/kernel.h>

int fsm_init(struct VCUState *state);
int fsm_update_state(struct VCUState *state, enum FSMTransitions transition);
