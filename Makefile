# SPDX-License-Identifier: GPL-2.0-only
VERSION = 0
PATCHLEVEL = 0
SUBLEVEL = 1
EXTRAVERSION :=
NAME =

ifeq ($(O),)
	O := .
endif

ifeq ($(V),1)
	override Q :=
	override S := @\#
else
	override Q := @
	override S := @
endif

override USER_CFLAGS := $(CFLAGS)
override USER_CXXFLAGS := $(CXXFLAGS)
override USER_LDFLAGS := $(LDFLAGS)
override USER_LIB_LDFLAGS := $(LIB_LDFLAGS)

all: config.make default

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
config.make: configure
	@if [ ! -e "$@" ]; then						\
	  echo "Running configure ...";					\
	  LDFLAGS="$(USER_LDFLAGS)"					\
	      LIB_LDFLAGS="$(USER_LIB_LDFLAGS)"				\
	      CFLAGS="$(USER_CFLAGS)" 					\
	      CXXFLAGS="$(USER_CXXFLAGS)"				\
	      ./configure;						\
	else								\
	  echo "$@ is out-of-date";					\
	  echo "Running configure ...";					\
	  LDFLAGS="$(USER_LDFLAGS)"					\
	      LIB_LDFLAGS="$(USER_LIB_LDFLAGS)"				\
	      CFLAGS="$(USER_CFLAGS)" 					\
	      CXXFLAGS="$(USER_CXXFLAGS)"				\
	      sed -n "/.*Configured with/s/[^:]*: //p" "$@" | sh;	\
	fi;


include config.make
endif
endif

override TARGET_BIN = $(O)/gwbot
override CONFIG_H_PATH = $(O)/config.h
override CONFIG_MAKE_PATH = $(O)/config.make
override CONFIG_LOG_PATH = $(O)/config.log

LD := $(CXX)
BASE_DIR	:= $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
BASE_DIR	:= $(strip $(patsubst %/, %, $(BASE_DIR)))
BASE_DEP_DIR	:= $(O)/.deps
MAKEFILE_FILE	:= $(lastword $(MAKEFILE_LIST))
INCLUDE_DIR	= -I$(BASE_DIR)
PACKAGE_NAME	:= $(TARGET_BIN)-$(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)
OBJ_CC		:=
OBJ_PRE_CC	:=
TARGET_BIN_CC	:=
DEP_DIRS	:= $(BASE_DEP_DIR)
DEPFLAGS	= -MT "$@" -MMD -MP -MF "$(@:$(BASE_DIR)/%.o=$(BASE_DEP_DIR)/%.d)"
EXT_DEP_FILE	:= $(MAKEFILE_FILE) $(CONFIG_H_PATH) $(CONFIG_MAKE_PATH)

CFLAGS		+= -DVERSION=$(VERSION) \
			-DPATCHLEVEL=$(PATCHLEVEL) \
			-DSUBLEVEL=$(SUBLEVEL) \
			-DEXTRAVERSION="\"$(EXTRAVERSION)\""

CXXFLAGS	+= -DVERSION=$(VERSION) \
			-DPATCHLEVEL=$(PATCHLEVEL) \
			-DSUBLEVEL=$(SUBLEVEL) \
			-DEXTRAVERSION="\"$(EXTRAVERSION)\""

include core/Makefile
include include/Makefile
include lib/Makefile
include modules/Makefile
include tests/Makefile

CFLAGS		+= $(INCLUDE_DIR)
CXXFLAGS	+= $(INCLUDE_DIR)

$(CONFIG_H_PATH): $(CONFIG_MAKE_PATH)

$(TARGET_BIN): $(EXT_DEP_FILE) $(OBJ_PRE_CC) $(OBJ_CC) $(TARGET_BIN_CC)
	$(S)echo "   LD		" "$(@:$(BASE_DIR)/%=%)";
	$(Q)$(LD) $(LDFLAGS) -o $(@) $(OBJ_PRE_CC) $(OBJ_CC) $(TARGET_BIN_CC) $(LIB_LDFLAGS);

OBJ_TESTS_CC += $(TARGET_TESTS:%.t=%.o)

$(TARGET_TESTS): $(EXT_DEP_FILE) $(OBJ_TESTS_CC) $(OBJ_PRE_CC) $(OBJ_CC) $(TARGET_BIN)
	$(S)echo "   LD		" "$(@:$(BASE_DIR)/%=%)";
	$(Q)$(LD) $(CFLAGS) $(LDFLAGS) -o $(@) $(@:%.t=%.o) $(OBJ_PRE_CC) $(OBJ_CC) $(LIB_LDFLAGS);

$(DEP_DIRS):
	$(S)echo "   MKDIR	" "$(@:$(BASE_DIR)/%=%)";
	$(Q)mkdir -p $(@);

$(OBJ_CC) $(OBJ_PRE_CC) $(OBJ_TESTS_CC) $(TARGET_BIN_CC): $(EXT_DEP_FILE) | $(DEP_DIRS)

%.o: %.c
	$(S)echo "   CC		" "$(@:$(BASE_DIR)/%=%)";
	$(Q)$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $(@) $(<);

%.o: %.cc
	$(S)echo "   CXX		" "$(@:$(BASE_DIR)/%=%)";
	$(Q)$(CXX) $(DEPFLAGS) $(CXXFLAGS) -c -o $(@) $(<);

%.o: %.cpp
	$(S)echo "   CXX		" "$(@:$(BASE_DIR)/%=%)";
	$(Q)$(CXX) $(DEPFLAGS) $(CXXFLAGS) -c -o $(@) $(<);

#
# Include generated dependencies data.
#
-include $(OBJ_CC:$(BASE_DIR)/%.o=$(BASE_DEP_DIR)/%.d)
-include $(OBJ_PRE_CC:$(BASE_DIR)/%.o=$(BASE_DEP_DIR)/%.d)

default: $(TARGET_BIN)

distclean: clean
	$(Q)rm -rfv $(CONFIG_H_PATH) $(CONFIG_MAKE_PATH) $(CONFIG_LOG_PATH);

clean: clean_tests
	$(Q)@rm -rfv $(DEP_DIRS) $(OBJ_CC)  $(shell find . -name "*.o");

.PHONY: default all distclean clean
