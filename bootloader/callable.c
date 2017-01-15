#define F_CPU 20000000
#include <avr/io.h>
#include <stdio.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

int getVersion(void) {
	return 2;
}

//Pageaddr is a byte address (aligned to SPM_PAGESIZE), pgmbuff is a
//array of bytes of size SPM_PAGESIZE. bytes.
void flashCopyPage(unsigned int pageaddr, unsigned char *pgmbuff) {
	int x;
	unsigned int w;
	eeprom_busy_wait();
	boot_page_erase(pageaddr);
	boot_spm_busy_wait();
	for (x=0; x<SPM_PAGESIZE; x+=2) {
		w=pgmbuff[x];
		w|=pgmbuff[x+1]<<8;
		boot_page_fill(pageaddr+x, w);
	}
	boot_page_write(pageaddr);
	boot_spm_busy_wait();
	boot_rww_enable();
}
