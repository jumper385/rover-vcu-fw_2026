#include "fsm_thread.h"
#include "fsm.h"
#include "ports/ports.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fsm_thread);

#define FSM_THREAD_STACK_SIZE 2048
#define FSM_THREAD_PRIORITY   4 
#define FSM_TICK_MS           10 

K_THREAD_STACK_DEFINE(fsm_thread_stack, FSM_THREAD_STACK_SIZE);
static struct k_thread fsm_thread_data;

/* max 5 lets us detect overruns without silently dropping ticks */
K_SEM_DEFINE(fsm_tick_sem, 0, 5);

static struct VCUState vcu_state;
static struct VCUPorts vcu_ports;

static void fsm_tick_handler(struct k_timer *timer) {
  ARG_UNUSED(timer);
  k_sem_give(&fsm_tick_sem);
}

K_TIMER_DEFINE(fsm_tick_timer, fsm_tick_handler, NULL);

static void fsm_thread_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  fsm_init(&vcu_state);
  k_timer_start(&fsm_tick_timer, K_MSEC(FSM_TICK_MS), K_MSEC(FSM_TICK_MS));

  for (;;) {
    k_sem_take(&fsm_tick_sem, K_FOREVER);

    if (k_sem_count_get(&fsm_tick_sem) > 0) {
      LOG_WRN("FSM tick overrun (%u tick(s) pending)",
              k_sem_count_get(&fsm_tick_sem));
    }

    ports_read_inputs(&vcu_ports);

    enum FSMTransitions tran = fsm_do_state(&vcu_state, &vcu_ports);

    ports_write_outputs(&vcu_ports);

    if (tran != TRAN_EVT_NONE) {
      fsm_update_state(&vcu_state, tran);
    }

    fsm_commit_state(&vcu_state);
  }
}

int fsm_thread_start(struct UDPTransport *udp_transport) {
  ports_init(&vcu_ports, udp_transport);

  k_thread_create(&fsm_thread_data, fsm_thread_stack,
                  K_THREAD_STACK_SIZEOF(fsm_thread_stack), fsm_thread_fn,
                  NULL, NULL, NULL, FSM_THREAD_PRIORITY, 0, K_NO_WAIT);

  k_thread_name_set(&fsm_thread_data, "fsm_control");

  LOG_INF("FSM control thread started (%d ms tick)", FSM_TICK_MS);
  return 0;
}
