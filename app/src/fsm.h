#pragma once
#include <zephyr/kernel.h>
#include "app_types.h"

int fsm_init(struct VCUState *state);
int update_state(struct VCUState *state, enum FSMTransitions transition);