ARCH	:=	-mthumb -mthumb-interwork
CFLAGS	:=	-g -Os -Wall -Wno-switch -Wno-multichar $(ARCH) $(INCLUDE) -finline-limit=10 -fno-unroll-loops -ffreestanding
ASFLAGS	:=	-g -Wa,--warn $(ARCH)
LDFLAGS :=  -nostdlib -T crtls/lnkscript -lgcc -static -L /usr/local/gcc_arm/gcc-arm-none-eabi-6_2-2016q4/lib/gcc/arm-none-eabi/6.2.1/
#PREFIX	:=	arm-linux-gnueabi-
PREFIX	:=	arm-none-eabi-
CC	:=	$(PREFIX)gcc
AS	:=	$(PREFIX)as
LD	:=	$(PREFIX)ld
AR	:=	$(PREFIX)ar
OBJCOPY	:=	$(PREFIX)objcopy
OBJS := main.o sound.o midinotefreq.o int.o serial.o tiles.o ui.o ccs.o seq.o gfxloader.o nvmem.o
#OBJS := code.o
#SRCS :=  main.c sound.c midinotefreq.c int.c serial.c tiles.c ui.c ccs.c seq.c gfxloader.c nvmem.c tonc_bios.s
#SRCS :=  main.c sound.c midinotefreq.c int.c serial.c tiles.c ui.c ccs.c seq.c gfxloader.c nvmem.c

gbamidi.gba: $(OBJS) crtls/crt0.o
#	$(LD) crtls/crt0.o $(OBJS) -o gbamidi.elf $(LDFLAGS)
	$(CC) -nostdlib  -Wl,-Tcrtls/lnkscript $(CFLAGS) -o gbamidi.elf crtls/crt0.o $(OBJS)  -ffreestanding -fwhole-program /usr/local/gcc_arm/gcc-arm-none-eabi-6_2-2016q4/lib/gcc/arm-none-eabi/6.2.1/libgcc.a 
	$(OBJCOPY) -O binary gbamidi.elf gbamidi.gba

tiles.c: tiles.png gentiles.php
	./gentiles.php > tiles.c

clean:
	rm -f *.o gbamidi.gba fwupdate.mid

