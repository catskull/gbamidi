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
GRIT=grit


gfx.a: brushedmetal_4bit.o
	$(AR) r gfx.a $(?)

brushedmetal_4bit.c:
	$(GRIT) $(*).png -ftc -fh -gb -gzl -ps 0 -pe 4

clean:
	rm -f *.c *.h *.o