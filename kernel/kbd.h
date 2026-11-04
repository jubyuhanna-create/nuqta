#ifndef KBD_H
#define KBD_H

/* non ascii keys come back above 0xff */
enum {
	KEY_UP = 0x100,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
};

void kbd_init(void);
int kbd_getc(void);	/* blocks */

#endif
