typedef int (*PF_RINT)(void);
typedef void (*PF_FLASHCOPYPAGE)(unsigned int, unsigned char *);

static __inline__ int call_getBootloaderVersion(void)
	{ return ((PF_RINT) (0x7fe0/2))(); }

//static __inline__ void call_flashCopyPage(unsigned int arg1, unsigned char *arg2)
//	{ ((PF_FLASHCOPYPAGE) (0x7fe2/2))(arg1, arg2); }


static __inline__ void call_flashCopyPage(unsigned int arg1, unsigned char *arg2)
	{ ((PF_FLASHCOPYPAGE) (0x7fe2/2))(arg1, arg2); }

