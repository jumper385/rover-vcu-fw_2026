#pragma once
#include "../app_types.h"
#include "../ports/ports.h"
#include <zephyr/kernel.h>

enum FSMTransitions booting_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions stopped_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions idle_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions execute_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions holding_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions held_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions stopping_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions suspending_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions suspended_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions aborting_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions aborted_handler(struct VCUState *state, struct VCUPorts *ports);
enum FSMTransitions clearing_handler(struct VCUState *state, struct VCUPorts *ports);
