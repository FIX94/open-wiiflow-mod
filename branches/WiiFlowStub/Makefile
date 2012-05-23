PREFIX = C:/devkitpro/devkitPPC/bin/powerpc-gekko-

# For a debug build, add -DDEBUG to CFLAGS
# Yeah I'm too lazy to automate this a better way :P

MACHDEP = -mcpu=750 -meabi -mhard-float -DTINY
CFLAGS = $(MACHDEP) -DDEBUG -Os -Wall -pipe -ffunction-sections -finline-functions-called-once -mno-sdata --combine -fwhole-program -ffreestanding
LDFLAGS = $(MACHDEP) -n -nostartfiles -nostdlib -Wl,-T,tinyload.ld
ASFLAGS = -D_LANGUAGE_ASSEMBLY -DHW_RVL -DTINY

TARGET_STRIP = stub.bin
TARGET = tinyload_sym.elf

CFILES = ios.c utils.c cache.c main.c
OBJS = crt0.o _all.o

export OBJCOPY	:=	$(PREFIX)objcopy

LIBS = 

include common.mk

all: $(TARGET_STRIP)

_all.o: $(CFILES)
	@echo "  COMPILE ALL"
	@mkdir -p $(DEPDIR)
	@$(CC) $(CFLAGS) $(DEFINES) -Wp,-MMD,$(DEPDIR)/$(*F).d,-MQ,"$@",-MP -c $(CFILES) -o $@


$(TARGET_STRIP): $(TARGET)
	@echo "  OBJCOPY   $@"
	$(Q)$(OBJCOPY) -O binary $< $@

upload: $(TARGET_STRIP)
	@WIILOAD=/dev/usbgecko wiiload $(TARGET_STRIP)

clean: myclean

myclean:
	rm -rf $(TARGET_STRIP)

