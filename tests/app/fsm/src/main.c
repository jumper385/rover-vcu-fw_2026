#include <zephyr/ztest.h>
#include "fsm.h"
#include "app_types.h"

ZTEST(fsm, test_init) {
    struct VCUState state;
    zassert_equal(fsm_init(&state), 0, "fsm_init returned error");
    zassert_equal(state.cs, FSM_BOOTING, "cs should be FSM_BOOTING after init");
    zassert_equal(state.ns, FSM_BOOTING, "ns should be FSM_BOOTING after init");
}

ZTEST_SUITE(fsm, NULL, NULL, NULL, NULL, NULL);