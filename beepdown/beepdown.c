#include "apilib.h"

void HariMain(void)
{
	int i, timer;
	timer = api_alloctimer();
	api_inittimer(timer, 128);
	for (i = 20000000; i >= 20000; i -= i / 100) {
		/* 20KHz�`20Hz : �l�Ԃɕ������鉹�͈̔� */
		/* i��1%�����炳��Ă��� */
		api_beep(i);
		// api_settimer(timer, 1);		/* 0.01�b */ // Old call
		api_settimer(timer, 1, 0);		/* 0.01秒, interval 0 */ // Correct call
		if (api_getkey(1) != 128) {
			break;
		}
	}
	api_beep(0);
	api_end();
}
