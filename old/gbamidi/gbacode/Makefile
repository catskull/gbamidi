ARCH	:=	-mthumb -mthumb-interwork
CFLAGS	:=	-g -Os -Wall -Wno-switch -Wno-multichar $(ARCH) $(INCLUDE) -finline-limit=10 -fno-unroll-loops -ffreestanding
ASFLAGS	:=	-g -Wa,--warn $(ARCH)
LDFLAGS :=  -nostdlib -T crtls/lnkscript -lgcc -static -L /home/jeroen/c/stm32/toolchain/lib/gcc/arm-none-eabi/4.5.2/
#PREFIX	:=	arm-linux-gnueabi-
PREFIX	:=	arm-none-eabi-
CC	:=	$(PREFIX)gcc
AS	:=	$(PREFIX)as
LD	:=	$(PREFIX)ld
AR	:=	$(PREFIX)ar
OBJCOPY	:=	$(PREFIX)objcopy
#OBJS := main.o sound.o midinotefreq.o int.o serial.o tiles.o ui.o ccs.o seq.o gfxloader.o
OBJS := code.o
SRCS :=  main.c sound.c midinotefreq.c int.c serial.c tiles.c ui.c ccs.c seq.c gfxloader.c


fwupdate.mid: gbafw2mid/gbafw2mid gbamidi.gba
	gbafw2mid/gbafw2mid fwupdate.mid < gbamidi.gba > /dev/null

gbafw2mid/gbafw2mid: gbafw2mid
	make -C gbafw2mid

gbamidi.gba: $(SRCS) crtls/crt0.o
#	$(LD) crtls/crt0.o $(OBJS) -o gbamidi.elf $(LDFLAGS)
	$(CC) -nostdlib  -Wl,-Tcrtls/lnkscript $(CFLAGS) -o gbamidi.elf crtls/crt0.o $(SRCS)  --combine -ffreestanding -fwhole-program /home/jeroen/c/stm32/toolchain/lib/gcc/arm-none-eabi/4.5.2/libgcc.a 
	$(OBJCOPY) -O binary gbamidi.elf gbamidi.gba

tiles.c: tiles.png gentiles.php
	./gentiles.php > tiles.c

clean:
	rm -f *.o gbamidi.gba fwupdate.mid

