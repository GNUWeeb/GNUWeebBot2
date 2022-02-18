
INCLUDE_DIR += -I$(BASE_DIR)/src
DEP_DIRS += $(BASE_DEP_DIR)/src

OBJ_TMP_CC := \
	$(BASE_DIR)/src/main.o

OBJ_PRE_CC += $(OBJ_TMP_CC)

$(OBJ_TMP_CC):
	$(CC_PRINT)
	$(Q)$(CC) $(PIE_FLAGS) $(DEPFLAGS) $(CFLAGS) -c $(O_TO_C) -o $(@)
