PROJECT = baudsetter

GCC_ARM_ON_PATH = $(shell command -v arm-none-eabi-gcc >/dev/null; echo $$?)

ifneq ($(GCC_ARM_ON_PATH),0)
GCC_BIN = ../dependencies/gcc-arm-embedded/bin/
endif

ifndef JTAG_INTERFACE
	JTAG_INTERFACE = olimex-arm-usb-ocd
endif

OPENOCD_CONF_BASE = ../conf/openocd
TARGET = $(BASE_TARGET)-lpc17xx
CMSIS_PATH = ./CDL/CMSISv2p00_LPC17xx
DRIVER_PATH = ./CDL/LPC17xxLib
INCLUDE_PATHS = -I. -I../atcommander -I$(DRIVER_PATH)/inc -I$(CMSIS_PATH)/inc \
				-IBSP -Iemqueue
ifeq ($(BOOTLOADER), 1)
LINKER_SCRIPT = LPC17xx-bootloader.ld
else
LINKER_SCRIPT = LPC17xx-baremetal.ld
endif

CC = $(GCC_BIN)arm-none-eabi-gcc
AS_FLAGS = -c -mcpu=cortex-m3 -mthumb --defsym RAM_MODE=0
CC_FLAGS = -c -fno-common -fmessage-length=0 -Wall -fno-exceptions \
		   -mcpu=cortex-m3 -mthumb -ffunction-sections -fdata-sections \
		   -Wno-char-subscripts -Wno-unused-but-set-variable -Werror -g -ggdb
CC_SYMBOLS += -DTOOLCHAIN_GCC_ARM -D__LPC17XX__ -DBOARD=9

AS = $(GCC_BIN)arm-none-eabi-as
LD = $(GCC_BIN)arm-none-eabi-gcc
LD_FLAGS = -mcpu=cortex-m3 -mthumb -Wl,--gc-sections
LD_SYS_LIBS = -lstdc++ -lsupc++ -lm -lc -lgcc

OBJCOPY = $(GCC_BIN)arm-none-eabi-objcopy

LOCAL_C_SRCS = $(wildcard *.c) $(wildcard ../atcommander/*.c)
LIB_C_SRCS += $(wildcard BSP/*.c)
LIB_C_SRCS += $(wildcard BSP/LPCXpressoBase_RevB/*.c)
LIB_C_SRCS += $(wildcard emqueue/*.c)
LIB_C_SRCS += $(CMSIS_PATH)/src/core_cm3.c
LIB_C_SRCS += $(CMSIS_PATH)/src/system_LPC17xx.c
LIB_C_SRCS += $(wildcard $(DRIVER_PATH)/src/*.c)
LOCAL_OBJ_FILES = $(LOCAL_C_SRCS:.c=.o) $(LIB_C_SRCS:.c=.o)

BSP_EXISTS = $(shell test -e BSP/bsp.h; echo $$?)
CDL_EXISTS = $(shell test -e CDL/README.mkd; echo $$?)
ifneq ($(BSP_EXISTS),0)
$(error BSP dependency is missing - did you run "git submodule init && git submodule update"?)
endif
ifneq ($(CDL_EXISTS),0)
$(error CDL dependency is missing - did you run "git submodule init && git submodule update"?)
endif

all: $(PROJECT).bin

clean:
	rm -f $(PROJECT).bin $(PROJECT).elf $(LOCAL_OBJ_FILES) *.o *.map

.s.o:
	$(AS)  $(CC_FLAGS) $(CC_SYMBOLS) -o $@ $<

.c.o:
	$(CC)  $(CC_FLAGS) $(CC_SYMBOLS) $(ONLY_C_FLAGS) $(INCLUDE_PATHS) -o $@ $<

$(PROJECT).elf: $(LOCAL_OBJ_FILES) $(SYS_OBJECTS)
	$(LD) $(LD_FLAGS) -T$(LINKER_SCRIPT) $(LIBRARY_PATHS) -o $@ $^ $(LIBRARIES) $(LD_SYS_LIBS) $(LIBRARIES) $(LD_SYS_LIBS)

$(PROJECT).bin: $(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

flash: all
	@openocd -f flash.cfg

gdb: all
	@openocd -f gdb.cfg
