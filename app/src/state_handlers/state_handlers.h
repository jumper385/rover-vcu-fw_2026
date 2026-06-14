#pragma once
#include "../app_types.h"
#include "../ports/ports.h"
#include <zephyr/kernel.h>

int booting_handler(struct VCUState *state, struct VCUPorts *ports);
int stopped_handler(struct VCUState *state, struct VCUPorts *ports);
int idle_handler(struct VCUState *state, struct VCUPorts *ports);
int execute_handler(struct VCUState *state, struct VCUPorts *ports);
int holding_handler(struct VCUState *state, struct VCUPorts *ports);
int held_handler(struct VCUState *state, struct VCUPorts *ports);
int stopping_handler(struct VCUState *state, struct VCUPorts *ports);
int suspending_handler(struct VCUState *state, struct VCUPorts *ports);
int suspended_handler(struct VCUState *state, struct VCUPorts *ports);
int aborting_handler(struct VCUState *state, struct VCUPorts *ports);
int aborted_handler(struct VCUState *state, struct VCUPorts *ports);
int clearing_handler(struct VCUState *state, struct VCUPorts *ports);
