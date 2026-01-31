# SPDX-License-Identifier: GPL-2.0

CFLAGS += -O3 -Wl,-no-as-needed -Wall -I $(top_srcdir)
LDLIBS += -lpthread -lm -luring

TEST_PROGS := test_generic_01.sh
TEST_PROGS += test_generic_02.sh
TEST_PROGS += test_generic_03.sh

TEST_PROGS += test_null_01.sh
TEST_PROGS += test_null_02.sh
TEST_PROGS += test_loop_01.sh
TEST_PROGS += test_loop_02.sh
TEST_PROGS += test_loop_03.sh
TEST_PROGS += test_loop_04.sh
TEST_PROGS += test_loop_05.sh
TEST_PROGS += test_stripe_01.sh
TEST_PROGS += test_stripe_02.sh
TEST_PROGS += test_stripe_03.sh
TEST_PROGS += test_stripe_04.sh

TEST_PROGS += test_stress_01.sh
TEST_PROGS += test_stress_02.sh

TEST_GEN_PROGS_EXTENDED = kublk

include ../lib.mk

$(TEST_GEN_PROGS_EXTENDED): kublk.c null.c file_backed.c common.c stripe.c

check:
	shellcheck -x -f gcc *.sh

# Test groups for running subsets of tests
# JOBS=1 (default): sequential with kselftest TAP output
# JOBS>1: parallel execution with xargs -P
# Usage: make run_null JOBS=4
JOBS ?= 1

# Auto-detect test groups from TEST_PROGS (test_<group>_<num>.sh -> group)
TEST_GROUPS := $(shell echo "$(TEST_PROGS)" | tr ' ' '\n' | \
	sed 's/test_\([^_]*\)_.*/\1/' | sort -u)

# Template for group test targets
# $(1) = group name (e.g., null, generic, stress)
define RUN_GROUP
run_$(1): all
	@if [ $$(JOBS) -gt 1 ]; then \
		echo $$(filter test_$(1)_%.sh,$$(TEST_PROGS)) | tr ' ' '\n' | \
			xargs -P $$(JOBS) -n1 sh -c './"$$$$0"' || true; \
	else \
		$$(call RUN_TESTS, $$(filter test_$(1)_%.sh,$$(TEST_PROGS))); \
	fi
.PHONY: run_$(1)
endef

# Generate targets for each discovered test group
$(foreach group,$(TEST_GROUPS),$(eval $(call RUN_GROUP,$(group))))

# Run all tests (parallel when JOBS>1)
run_all: all
	@if [ $(JOBS) -gt 1 ]; then \
		echo $(TEST_PROGS) | tr ' ' '\n' | \
			xargs -P $(JOBS) -n1 sh -c './"$$0"' || true; \
	else \
		$(call RUN_TESTS, $(TEST_PROGS)); \
	fi
.PHONY: run_all
