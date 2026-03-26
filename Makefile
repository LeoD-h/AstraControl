# ==============================================================================
# Makefile - Projet Fusée
# VM (192.168.64.7)    : satellite_server + controle_fusee_data
# JoyPi (192.168.1.21) : joypi_controller + controle_fusee
# ==============================================================================

BIN_DIR       = bin
BIN_UTIL_DIR  = $(BIN_DIR)/bin-util
BIN_TEST_DIR  = $(BIN_DIR)/tests

VM_DIR    = To-VM
JOYPI_DIR = To-JoyPI

# Cross-compilateur ARM (JoyPi et RPi)
PATH_CC          = /home/leo/Desktop/CCR/tools-master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin
CC_CROSS         = $(PATH_CC)/arm-linux-gnueabihf-gcc
CC_NATIVE        = gcc
READELF_CROSS    = $(PATH_CC)/arm-linux-gnueabihf-readelf

# wiringPi cross-compilé pour JoyPi/RPi (ARM ELF)
WIRINGPI_ROOT = /home/leo/Desktop/Objet_Connecte/wiringPi-36fb7f1
WIRINGPI_INC  = -I$(WIRINGPI_ROOT)/wiringPi
WIRINGPI_LDIR = -L$(WIRINGPI_ROOT)/wiringPi

# ncurses cross-compilé pour JoyPi/RPi
JOYPI_NCURSES_ROOT = /home/leo/Desktop/CCR/ncurses-lab/target_NC_PI
JOYPI_NCURSES_INC  = -I$(JOYPI_NCURSES_ROOT)/include
JOYPI_NCURSES_LDIR = -L$(JOYPI_NCURSES_ROOT)/lib
JOYPI_NCURSES_LIBS = -lncursesw

# Flags communs
CFLAGS_BASE = -std=gnu99 -Wall -Wextra -O2

# Sources
SRC_SATELLITE    = Network/satellite_server.c Network/satellite_handler.c
SRC_DATA_VM      = Dashboard/data_input_text.c Dashboard/data_gen_model.c Dashboard/dashboard_common.c
SRC_DASHBOARD    = Dashboard/main.c Dashboard/dashboard_logic.c Dashboard/dashboard_dynamics.c \
                   Dashboard/dashboard_visuals.c Dashboard/pipes.c
SRC_CONTROLLER   = JoyPi/joypi_controller.c JoyPi/joypi_ctrl_net.c JoyPi/joypi_ctrl_keys.c \
                   JoyPi/joypi_ctrl_actions.c JoyPi/actuators.c JoyPi/actuators_display.c \
                   JoyPi/ir_input.c
SRC_BTN_TEST     = JoyPi/tests/hardware_buttons_test.c
SRC_ACT_TEST     = JoyPi/tests/hardware_actuators_test.c
SRC_STOP_TEST    = JoyPi/tests/hardware_stop.c
SRC_IR_TEST      = JoyPi/tests/joypi_ir_test.c
SRC_MOTOR_TEST   = JoyPi/tests/moteur.c
SRC_HW_BTN_TEST  = JoyPi/tests/joypi_hardware_button.c

# Binaires natifs (x86, pour tests VM)
SAT_NATIVE         = $(BIN_UTIL_DIR)/satellite_server
DATA_VM_NATIVE     = $(BIN_UTIL_DIR)/controle_fusee_data
DASHBOARD_NATIVE   = $(BIN_UTIL_DIR)/controle_fusee_x86

# Binaires cross-compilés RPi (pour déploiement VM→RPi)
SAT_RPI          = $(BIN_UTIL_DIR)/satellite_server_rpi
DATA_VM_RPI      = $(BIN_UTIL_DIR)/controle_fusee_data_rpi

# Binaires cross-compilés JoyPi (ARM, avec ncurses cross)
DASHBOARD_JOYPI   = $(BIN_UTIL_DIR)/controle_fusee_joypi
CONTROLLER_JOYPI  = $(BIN_UTIL_DIR)/joypi_controller_joypi
BTN_TEST_JOYPI    = $(BIN_TEST_DIR)/hardware_buttons_test_joypi
ACT_TEST_JOYPI    = $(BIN_TEST_DIR)/hardware_actuators_test_joypi
STOP_TEST_JOYPI   = $(BIN_TEST_DIR)/hardware_stop_joypi
IR_TEST_JOYPI     = $(BIN_TEST_DIR)/ir_test_joypi
MOTOR_TEST_JOYPI  = $(BIN_TEST_DIR)/moteur_test_joypi
HW_BTN_TEST_JOYPI = $(BIN_TEST_DIR)/hw_button_test_joypi

ifneq ($(V),1)
Q ?= @
endif

.PHONY: all vm joypi clean

# ------------ Cibles principales ------------

all: vm joypi

# -- VM (satellite) --
# Compile en natif x86 ET en cross ARM RPi + dashboard x86 pour test local
vm: $(BIN_UTIL_DIR) \
    $(SAT_NATIVE) $(DATA_VM_NATIVE) $(DASHBOARD_NATIVE) \
    $(SAT_RPI) $(DATA_VM_RPI)
	$(Q)rm -rf $(VM_DIR)
	$(Q)mkdir -p $(VM_DIR)/bin-util
	$(Q)cp -f $(SAT_NATIVE)       $(VM_DIR)/bin-util/satellite_server
	$(Q)cp -f $(DATA_VM_NATIVE)   $(VM_DIR)/bin-util/controle_fusee_data
	$(Q)cp -f $(DASHBOARD_NATIVE) $(VM_DIR)/bin-util/controle_fusee
	$(Q)cp -f $(SAT_RPI)          $(VM_DIR)/bin-util/satellite_server_rpi
	$(Q)cp -f $(DATA_VM_RPI)      $(VM_DIR)/bin-util/controle_fusee_data_rpi
	@echo "VM ready: $(VM_DIR)/"
	@echo "  satellite_server        <- x86 (pour VM)"
	@echo "  controle_fusee_data     <- x86 (injecteur)"
	@echo "  controle_fusee          <- x86 (dashboard ncurses, test local)"
	@echo "  satellite_server_rpi    <- ARM (pour RPi futur)"
	@echo "  controle_fusee_data_rpi <- ARM"

# -- JoyPi (contrôleur) --
# Cross-compile ARM avec wiringPi + ncurses
joypi: $(BIN_UTIL_DIR) $(BIN_TEST_DIR) \
	$(DASHBOARD_JOYPI) $(CONTROLLER_JOYPI) \
	$(BTN_TEST_JOYPI) $(ACT_TEST_JOYPI) $(STOP_TEST_JOYPI) \
	$(IR_TEST_JOYPI) $(MOTOR_TEST_JOYPI) $(HW_BTN_TEST_JOYPI)
	$(Q)rm -rf $(JOYPI_DIR)
	$(Q)mkdir -p $(JOYPI_DIR)/bin-util $(JOYPI_DIR)/tests $(JOYPI_DIR)/lib
	$(Q)cp -f $(DASHBOARD_JOYPI)   $(JOYPI_DIR)/bin-util/controle_fusee
	$(Q)cp -f $(CONTROLLER_JOYPI)  $(JOYPI_DIR)/bin-util/joypi_controller
	$(Q)cp -f $(BTN_TEST_JOYPI)    $(JOYPI_DIR)/tests/hardware_buttons_test
	$(Q)cp -f $(ACT_TEST_JOYPI)    $(JOYPI_DIR)/tests/hardware_actuators_test
	$(Q)cp -f $(STOP_TEST_JOYPI)   $(JOYPI_DIR)/tests/hardware_stop
	$(Q)cp -f $(IR_TEST_JOYPI)     $(JOYPI_DIR)/tests/ir_test
	$(Q)cp -f $(MOTOR_TEST_JOYPI)  $(JOYPI_DIR)/tests/moteur_test
	$(Q)cp -f $(HW_BTN_TEST_JOYPI) $(JOYPI_DIR)/tests/hw_button_test
	$(Q)cp -f $(JOYPI_NCURSES_ROOT)/lib/libncursesw.so.6.6 $(JOYPI_DIR)/lib/ 2>/dev/null || true
	$(Q)ln -sf libncursesw.so.6.6 $(JOYPI_DIR)/lib/libncursesw.so.6 2>/dev/null || true
	$(Q)ln -sf libncursesw.so.6.6 $(JOYPI_DIR)/lib/libncurses.so.6 2>/dev/null || true
	$(Q)mkdir -p $(JOYPI_DIR)/share/terminfo/x $(JOYPI_DIR)/share/terminfo/l $(JOYPI_DIR)/share/terminfo/v
	$(Q)cp -f $(JOYPI_NCURSES_ROOT)/share/terminfo/x/xterm   $(JOYPI_DIR)/share/terminfo/x/
	$(Q)cp -f $(JOYPI_NCURSES_ROOT)/share/terminfo/l/linux   $(JOYPI_DIR)/share/terminfo/l/
	$(Q)cp -f $(JOYPI_NCURSES_ROOT)/share/terminfo/v/vt100   $(JOYPI_DIR)/share/terminfo/v/
	$(Q)cp -f $(WIRINGPI_ROOT)/wiringPi/libwiringPi.so.2.50 $(JOYPI_DIR)/lib/ 2>/dev/null || true
	$(Q)ln -sf libwiringPi.so.2.50 $(JOYPI_DIR)/lib/libwiringPi.so.2 2>/dev/null || true
	$(Q)ln -sf libwiringPi.so.2.50 $(JOYPI_DIR)/lib/libwiringPi.so 2>/dev/null || true
	$(Q)printf '%s\n' \
		'#!/usr/bin/env sh' \
		'D=$$(cd "$$(dirname "$$0")" && pwd)' \
		'export LD_LIBRARY_PATH="$$D/lib$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}"' \
		'export TERM=xterm' \
		'export TERMINFO="$$D/share/terminfo"' \
		'exec "$$D/bin-util/controle_fusee" "$$@"' \
		> $(JOYPI_DIR)/run_controle_fusee.sh
	$(Q)printf '%s\n' \
		'#!/usr/bin/env sh' \
		'D=$$(cd "$$(dirname "$$0")" && pwd)' \
		'export LD_LIBRARY_PATH="$$D/lib$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}"' \
		'export TERM=xterm' \
		'export TERMINFO="$$D/share/terminfo"' \
		'exec "$$D/bin-util/joypi_controller" "$$@"' \
		> $(JOYPI_DIR)/run_controller.sh
	$(Q)printf '%s\n' \
		'#!/usr/bin/env sh' \
		'# Source: . ./joypi_env.sh  (ou utiliser run_*.sh directement)' \
		'D=$$(cd "$$(dirname "$$0")" && pwd)' \
		'export LD_LIBRARY_PATH="$$D/lib$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}"' \
		'export TERM=xterm' \
		'export TERMINFO="$$D/share/terminfo"' \
		'echo "env OK: TERMINFO=$$TERMINFO  TERM=$$TERM"' \
		> $(JOYPI_DIR)/joypi_env.sh
	$(Q)chmod +x $(JOYPI_DIR)/run_controle_fusee.sh $(JOYPI_DIR)/run_controller.sh $(JOYPI_DIR)/joypi_env.sh
	@echo "JoyPi ready: $(JOYPI_DIR)/"
	@echo "  bin-util/controle_fusee   <- ARM ncurses dashboard"
	@echo "  bin-util/joypi_controller <- ARM client satellite (wiringPi)"
	@echo "  tests/                    <- binaires de test hardware"

# ------------ Règles de compilation ------------

$(BIN_UTIL_DIR):
	$(Q)mkdir -p $(BIN_UTIL_DIR)

$(BIN_TEST_DIR):
	$(Q)mkdir -p $(BIN_TEST_DIR)

# VM : satellite_server x86
$(SAT_NATIVE): $(SRC_SATELLITE) | $(BIN_UTIL_DIR)
	$(Q)$(CC_NATIVE) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi -o $@ $^
	@echo "  [x86]  satellite_server"

# VM : satellite_server ARM RPi
$(SAT_RPI): $(SRC_SATELLITE) | $(BIN_UTIL_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi -o $@ $^
	@echo "  [ARM]  satellite_server_rpi"

# VM : controle_fusee_data x86
$(DATA_VM_NATIVE): $(SRC_DATA_VM) | $(BIN_UTIL_DIR)
	$(Q)$(CC_NATIVE) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi -o $@ $^
	@echo "  [x86]  controle_fusee_data"

# VM : controle_fusee_data ARM RPi
$(DATA_VM_RPI): $(SRC_DATA_VM) | $(BIN_UTIL_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi -o $@ $^
	@echo "  [ARM]  controle_fusee_data_rpi"

# VM : controle_fusee x86 (dashboard ncurses natif, pour test local sur VM)
$(DASHBOARD_NATIVE): $(SRC_DASHBOARD) | $(BIN_UTIL_DIR)
	$(Q)$(CC_NATIVE) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi -o $@ $^ -lncurses
	@echo "  [x86]  controle_fusee (dashboard ncurses)"

# JoyPi : controle_fusee (dashboard ncurses ARM)
$(DASHBOARD_JOYPI): $(SRC_DASHBOARD) | $(BIN_UTIL_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi $(JOYPI_NCURSES_INC) -o $@ $^ $(JOYPI_NCURSES_LDIR) $(JOYPI_NCURSES_LIBS)
	@echo "  [ARM]  controle_fusee_joypi"

# JoyPi : joypi_controller (ARM, wiringPi)
$(CONTROLLER_JOYPI): $(SRC_CONTROLLER) | $(BIN_UTIL_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi $(WIRINGPI_INC) -DUSE_WIRINGPI -o $@ $^ $(WIRINGPI_LDIR) -lwiringPi -ldl
	@echo "  [ARM]  joypi_controller_joypi"

# JoyPi : hardware tests (ARM, wiringPi)
$(BTN_TEST_JOYPI): $(SRC_BTN_TEST) | $(BIN_TEST_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi $(WIRINGPI_INC) -DUSE_WIRINGPI -o $@ $^ $(WIRINGPI_LDIR) -lwiringPi
	@echo "  [ARM]  hardware_buttons_test"

$(ACT_TEST_JOYPI): $(SRC_ACT_TEST) | $(BIN_TEST_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi $(WIRINGPI_INC) -DUSE_WIRINGPI -o $@ $^ $(WIRINGPI_LDIR) -lwiringPi
	@echo "  [ARM]  hardware_actuators_test"

$(STOP_TEST_JOYPI): $(SRC_STOP_TEST) | $(BIN_TEST_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi $(WIRINGPI_INC) -DUSE_WIRINGPI -o $@ $^ $(WIRINGPI_LDIR) -lwiringPi
	@echo "  [ARM]  hardware_stop"

$(IR_TEST_JOYPI): $(SRC_IR_TEST) | $(BIN_TEST_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi $(WIRINGPI_INC) -DUSE_WIRINGPI -o $@ $^ $(WIRINGPI_LDIR) -lwiringPi
	@echo "  [ARM]  ir_test"

$(MOTOR_TEST_JOYPI): $(SRC_MOTOR_TEST) | $(BIN_TEST_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IDashboard -INetwork -IJoyPi $(WIRINGPI_INC) -DUSE_WIRINGPI -o $@ $^ $(WIRINGPI_LDIR) -lwiringPi
	@echo "  [ARM]  moteur_test"

$(HW_BTN_TEST_JOYPI): $(SRC_HW_BTN_TEST) | $(BIN_TEST_DIR)
	$(Q)$(CC_CROSS) $(CFLAGS_BASE) -IJoyPi -IJoyPi/tests $(WIRINGPI_INC) -DUSE_WIRINGPI -DHW_BUTTON_STANDALONE -o $@ $^ $(WIRINGPI_LDIR) -lwiringPi -ldl
	@echo "  [ARM]  hw_button_test"


# ------------ Nettoyage ------------

clean:
	$(Q)rm -rf $(BIN_DIR) $(VM_DIR) $(JOYPI_DIR)
	@echo "Cleaned."
