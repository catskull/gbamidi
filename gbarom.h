
extern unsigned char _binary_gbarom_bin_start;
extern unsigned char _binary_gbarom_bin_end;

#define gbarom_data (&_binary_gbarom_bin_start)
#define gbarom_size (&_binary_gbarom_bin_end-&_binary_gbarom_bin_start)
