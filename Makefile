
PREFIX = C:/devkitpro/devkitPPC/bin/powerpc-gekko-

AR = $(PREFIX)ar
AS = $(PREFIX)as
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
RANLIB = $(PREFIX)ranlib
STRIP = $(PREFIX)strip

MACHDEP = -mcpu=750 -meabi -mhard-float -DTINY
CFLAGS = $(MACHDEP) -Os -Wall -pipe -ffunction-sections -finline-functions-called-once -mno-sdata --combine -fwhole-program -ffreestanding
LDFLAGS = $(MACHDEP) -n -nostartfiles -nostdlib -Wl,-T,tinyload.ld
ASFLAGS = -D_LANGUAGE_ASSEMBLY -DHW_RVL -DTINY

TARGET_LINKED = tinyload.elf
TARGET = stub.bin

CFILES = ios.c utils.c cache.c main.c
OBJS = crt0.o _all.o

DEPDIR = .deps

LIBS = 

all: $(TARGET)

%.o: %.s
	@echo " ASSEMBLE    $<"
	@$(CC) $(CFLAGS) $(DEFINES) $(ASFLAGS) -c $< -o $@

%.o: %.S
	@echo " ASSEMBLE    $<"
	@$(CC) $(CFLAGS) $(DEFINES) $(ASFLAGS) -c $< -o $@

_all.o: $(CFILES)
	@echo " COMPILE ALL "
	@mkdir -p $(DEPDIR)
	@$(CC) $(CFLAGS) $(DEFINES) -Wp,-MMD,$(DEPDIR)/$(*F).d,-MQ,"$@",-MP -c $(CFILES) -o $@

$(TARGET_LINKED): $(OBJS)
	@echo " LINK        $@"
	@$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(TARGET): $(TARGET_LINKED)
	@echo " OBJCOPY     $@"
	@$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(TARGET_LINKED) $(TARGET) $(OBJS) $(DEPDIR)
