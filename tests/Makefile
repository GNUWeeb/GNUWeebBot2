# SPDX-License-Identifier: GPL-2.0-only

DEP_DIRS += $(BASE_DEP_DIR)/tests
TARGET_TESTS :=

include $(BASE_DIR)/tests/lib/Makefile
include $(BASE_DIR)/tests/core/Makefile

$(TARGET_TESTS): $(OBJ_CC)

tests: $(TARGET_TESTS)

clean_tests:
	$(Q)rm -vf $(TARGET_TESTS)

.PHONY: test clean_test
