/* bootpack�̃��C�� */

#include "bootpack.h"
#include <stdio.h>

#define KEYCMD_LED		0xed

void keywin_off(struct SHEET *key_win);
void keywin_on(struct SHEET *key_win);
void close_console(struct SHEET *sht);
void close_constask(struct TASK *task);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct SHTCTL *shtctl;
	char s[40];
	struct FIFO32 fifo, keycmd;
	int fifobuf[128], keycmd_buf[32];
	int mx, my, i, new_mx = -1, new_my = 0, new_wx = 0x7fffffff, new_wy = 0;
	unsigned int memtotal;
	struct MOUSE_DEC mdec;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	unsigned char *buf_back, buf_mouse[256];
	struct SHEET *sht_back, *sht_mouse;
	struct TASK *task_a, *task;
	static char keytable0[0x80] = {
		0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0x08, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0x0a, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0,   0,   ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
	static char keytable1[0x80] = {
		0,   0,   '!', 0x22, '#', '$', '%', '&', 0x27, '(', ')', '~', '=', '~', 0x08, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '`', '{', 0x0a, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '*', 0,   0,   '}', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   '_', 0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
	};
	int key_shift = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;
	int key_ctrl = 0; // 添加 Ctrl 键状态变量
	static char key_pressed[0x80] = {0}; // 添加：跟踪按键状态 (0: released, 1: pressed)
	int j, x, y, mmx = -1, mmy = -1, mmx2 = 0;
	struct SHEET *sht = 0, *key_win, *sht2;
	int *fat;
	unsigned char *nihongo;
	struct FILEINFO *finfo;
	extern char hankaku[4096];

	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC�̏��������I������̂�CPU�̊��荞�݋֎~������ */
	fifo32_init(&fifo, 128, fifobuf, 0);
	*((int *) 0x0fec) = (int) &fifo;
	init_pit();
	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	io_out8(PIC0_IMR, 0xf8); /* PIT��PIC1�ƃL�[�{�[�h������(11111000) */
	io_out8(PIC1_IMR, 0xef); /* �}�E�X������(11101111) */
	fifo32_init(&keycmd, 32, keycmd_buf, 0);

	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	
	// 初始化内存磁盘
	init_memory_disk(memman);

	init_palette();
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	task_a = task_init(memman);
	fifo.task = task_a;
	task_run(task_a, 1, 2);
	*((int *) 0x0fe4) = (int) shtctl;
	task_a->langmode = 0;

	/* sht_back */
	sht_back  = sheet_alloc(shtctl);
	buf_back  = (unsigned char *) memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1); /* �����F�Ȃ� */
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);
	boxfill8(buf_back, binfo->scrnx, COL8_C6C6C6, 0,         binfo->scrny - 28, binfo->scrnx - 1, binfo->scrny - 28);
	boxfill8(buf_back, binfo->scrnx, COL8_FFFFFF, 0,         binfo->scrny - 27, binfo->scrnx - 1, binfo->scrny - 27);
	boxfill8(buf_back, binfo->scrnx, COL8_C6C6C6, 0,         binfo->scrny - 26, binfo->scrnx - 1, binfo->scrny -  1);

	boxfill8(buf_back, binfo->scrnx, COL8_FFFFFF, 3,         binfo->scrny - 24, 59,        binfo->scrny - 24);
	boxfill8(buf_back, binfo->scrnx, COL8_FFFFFF, 2,         binfo->scrny - 24,  2,        binfo->scrny -  4);
	boxfill8(buf_back, binfo->scrnx, COL8_848484, 3,         binfo->scrny -  4, 59,        binfo->scrny -  4);
	boxfill8(buf_back, binfo->scrnx, COL8_848484, 59,        binfo->scrny - 23, 59,        binfo->scrny -  5);
	boxfill8(buf_back, binfo->scrnx, COL8_000000, 2,         binfo->scrny -  3, 59,        binfo->scrny -  3);
	boxfill8(buf_back, binfo->scrnx, COL8_000000, 60,        binfo->scrny - 24, 60,        binfo->scrny -  3);

	/* 在按钮区域绘制 "START" 文本 */
	/* "START" 长度为 5, 宽度 5*8=40. 按钮区域宽度约 58 (2 to 59). x = (58-40)/2 + 2 = 9 + 2 = 11 */
	/* 字体高度 16. 按钮区域高度约 20 (scrny-24 to scrny-4). y = (20-16)/2 + (scrny-24) = 2 + scrny - 24 = scrny - 22 */
	putfonts8_asc_sht(sht_back, 11, binfo->scrny - 22, COL8_000000, COL8_C6C6C6, "START", 5);

	boxfill8(buf_back, binfo->scrnx, COL8_848484, binfo->scrnx - 47, binfo->scrny - 24, binfo->scrnx -  4, binfo->scrny - 24);

	/* sht_cons */
	key_win = open_console(shtctl, memtotal);

	/* sht_mouse */
	sht_mouse = sheet_alloc(shtctl);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
	init_mouse_cursor8(buf_mouse, 99);
	mx = (binfo->scrnx - 16) / 2; /* ��ʒ����ɂȂ�悤�ɍ��W�v�Z */
	my = (binfo->scrny - 28 - 16) / 2;

	sheet_slide(sht_back,  0,  0);
	sheet_slide(key_win,   32, 4);
	sheet_slide(sht_mouse, mx, my);
	sheet_updown(sht_back,  0);
	sheet_updown(key_win,   1);
	sheet_updown(sht_mouse, 2);
	keywin_on(key_win);

	/* �ŏ��ɃL�[�{�[�h��ԂƂ̐H���Ⴂ���Ȃ��悤�ɁA�ݒ肵�Ă������Ƃɂ��� */
	fifo32_put(&keycmd, KEYCMD_LED);
	fifo32_put(&keycmd, key_leds);

	/* nihongo.fnt�̓ǂݍ��� */
	fat = (int *) memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));

	finfo = file_search("nihongo.fnt", (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo != 0) {
		i = finfo->size;
		nihongo = file_loadfile2(finfo->clustno, &i, fat);
	} else {
		nihongo = (unsigned char *) memman_alloc_4k(memman, 16 * 256 + 32 * 94 * 47);
		for (i = 0; i < 16 * 256; i++) {
			nihongo[i] = hankaku[i]; /* �t�H���g���Ȃ������̂Ŕ��p�������R�s�[ */
		}
		for (i = 16 * 256; i < 16 * 256 + 32 * 94 * 47; i++) {
			nihongo[i] = 0xff; /* �t�H���g���Ȃ������̂őS�p������0xff�Ŗ��ߐs���� */
		}
	}
	*((int *) 0x0fe8) = (int) nihongo;
	memman_free_4k(memman, (int) fat, 4 * 2880);

	for (;;) {
		if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) {
			/* �L�[�{�[�h�R���g���[���ɑ���f�[�^������΁A���� */
			keycmd_wait = fifo32_get(&keycmd);
			wait_KBC_sendready();
			io_out8(PORT_KEYDAT, keycmd_wait);
		}
		io_cli();
		if (fifo32_status(&fifo) == 0) {
			/* FIFO��������ۂɂȂ����̂ŁA�ۗ����Ă���`�悪����Ύ��s���� */
			if (new_mx >= 0) {
				io_sti();
				sheet_slide(sht_mouse, new_mx, new_my);
				new_mx = -1;
			} else if (new_wx != 0x7fffffff) {
				io_sti();
				sheet_slide(sht, new_wx, new_wy);
				new_wx = 0x7fffffff;
			} else {
				task_sleep(task_a);
				io_sti();
			}
		} else {
			i = fifo32_get(&fifo);
			io_sti();
			if (key_win != 0 && key_win->flags == 0) {	/* EBhEꂽ */
				if (shtctl->top == 1) {	/* �����}�E�X�Ɣw�i�����Ȃ� */
					key_win = 0;
				} else {
					key_win = shtctl->sheets[shtctl->top - 1];
					keywin_on(key_win);
				}
			}
			if (256 <= i && i <= 511) { /* キーボードデータ */
				int sc = i - 256; // Scan code (0x00 - 0xff)
                int press_code;
                char translated_char = 0; // 用于存储转换后的字符

                if (sc < 0x80) { // 按下码 (Make Code)
                    press_code = sc;
                    if (key_pressed[press_code] == 0) { // 首次按下
                        key_pressed[press_code] = 1; // 标记为按下

                        // --- 字符转换逻辑 ---
					if (key_shift == 0) {
                            translated_char = keytable0[press_code];
				} else {
                            translated_char = keytable1[press_code];
				}
                        if ('A' <= translated_char && translated_char <= 'Z') {
					if (((key_leds & 4) == 0 && key_shift == 0) ||
							((key_leds & 4) != 0 && key_shift != 0)) {
                                translated_char += 0x20; /* 大写转小写 */
					}
				}
                        // --- 字符转换逻辑结束 ---

                        // --- 发送字符到活动窗口 ---
                        if (translated_char != 0 && key_win != 0) {
                            // 特殊处理 Ctrl+C (不发送 'C')
                            if (!(key_ctrl != 0 && press_code == 0x2e)) {
                                fifo32_put(&key_win->task->fifo, translated_char + 256);
				}
                        }

                        // --- 处理特殊按键的按下状态 ---
                        if (press_code == 0x2a) { key_shift |= 1; } // Left Shift ON
                        if (press_code == 0x36) { key_shift |= 2; } // Right Shift ON
                        if (press_code == 0x1d) { key_ctrl = 1; }    // Left Ctrl ON (Right Ctrl has different code if needed)

                        if (press_code == 0x3a) { /* CapsLock */
					key_leds ^= 4;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
                        if (press_code == 0x45) { /* NumLock */
					key_leds ^= 2;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
                        if (press_code == 0x46) { /* ScrollLock */
					key_leds ^= 1;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}

                        // --- 处理功能键和组合键 ---
                        if (press_code == 0x0f && key_win != 0) { /* Tab */
                            keywin_off(key_win);
                            j = key_win->height - 1;
                            if (j == 0) { j = shtctl->top - 1; }
                            key_win = shtctl->sheets[j];
                            keywin_on(key_win);
                        }
                        if (press_code == 0x3b && key_shift != 0 && key_win != 0) {	/* Shift+F1 */
					task = key_win->task;
					if (task != 0 && task->tss.ss0 != 0) {
						cons_putstr0(task->cons, "\nBreak(key) :\n");
                                io_cli();	/* 強制終了処理中にタスクが変わると困るから */
						task->tss.eax = (int) &(task->tss.esp0);
						task->tss.eip = (int) asm_end_app;
						io_sti();
                                task_run(task, -1, 0);	/* 終了処理を確実にやらせるために、寝ていたら起こす */
					}
				}
                        if (press_code == 0x3c && key_shift != 0) {	/* Shift+F2 */
                            /* 新しく作ったコンソールをアクティブにする（前のほうが見やすいか？） */
					if (key_win != 0) {
						keywin_off(key_win);
					}
					key_win = open_console(shtctl, memtotal);
					sheet_slide(key_win, 32, 4);
					sheet_updown(key_win, shtctl->top);
					keywin_on(key_win);
				}
                        if (press_code == 0x57) {	/* F11 */
					sheet_updown(shtctl->sheets[1], shtctl->top - 1);
				}
                        if (key_ctrl != 0 && press_code == 0x2e) { // Ctrl + C is pressed
                            // 打开新控制台
                            if (key_win != 0) {
                                keywin_off(key_win); // 取消当前窗口激活状态
                            }
                            key_win = open_console(shtctl, memtotal);
                            sheet_slide(key_win, 32, 4);
                            sheet_updown(key_win, shtctl->top);
                            keywin_on(key_win); // 激活新窗口
                        }
                        // ... 其他特殊按键处理 ...

                    } else {
                        // 按键已被按下，忽略重复的按下码 (防止 IME 重复)
                        // 如果需要实现按键重复功能 (key repeat)，可以在这里添加计时器逻辑
                    }
                } else { // 释放码 (Break Code)
                    press_code = sc - 0x80;
                    key_pressed[press_code] = 0; // 标记为释放

                    // --- 处理特殊按键的释放状态 ---
                    if (press_code == 0x2a) { key_shift &= ~1; } // Left Shift OFF
                    if (press_code == 0x36) { key_shift &= ~2; } // Right Shift OFF
                    if (press_code == 0x1d) { key_ctrl = 0; }    // Left Ctrl OFF
                }

                // --- 处理键盘控制器响应码 (与按键状态无关) ---
                if (sc == 0xfa) { /* キーボードがデータを無事に受け取った */
					keycmd_wait = -1;
				}
                if (sc == 0xfe) { /* キーボードがデータを無事に受け取れなかった */
					wait_KBC_sendready();
					io_out8(PORT_KEYDAT, keycmd_wait);
				}
			} else if (512 <= i && i <= 767) { /* �}�E�X�f�[�^ */
				if (mouse_decode(&mdec, i - 512) != 0) {
					/* �}�E�X�J�[�\���̈ړ� */
					mx += mdec.x;
					my += mdec.y;
					if (mx < 0) {
						mx = 0;
					}
					if (my < 0) {
						my = 0;
					}
					if (mx > binfo->scrnx - 1) {
						mx = binfo->scrnx - 1;
					}
					if (my > binfo->scrny - 1) {
						my = binfo->scrny - 1;
					}
					new_mx = mx;
					new_my = my;
					if ((mdec.btn & 0x01) != 0) {
						/* ���{�^���������Ă��� */
						if (mmx < 0) {
							/* �ʏ탂�[�h�̏ꍇ */
							/* ��̉��������珇�ԂɃ}�E�X���w���Ă��鉺������T�� */
							for (j = shtctl->top - 1; j > 0; j--) {
								sht = shtctl->sheets[j];
								x = mx - sht->vx0;
								y = my - sht->vy0;
								if (0 <= x && x < sht->bxsize && 0 <= y && y < sht->bysize) {
									if (sht->buf[y * sht->bxsize + x] != sht->col_inv) {
										sheet_updown(sht, shtctl->top - 1);
										if (sht != key_win) {
											keywin_off(key_win);
											key_win = sht;
											keywin_on(key_win);
										}
										if (3 <= x && x < sht->bxsize - 3 && 3 <= y && y < 21) {
											mmx = mx;	/* �E�B���h�E�ړ����[�h�� */
											mmy = my;
											mmx2 = sht->vx0;
											new_wy = sht->vy0;
										}
										if (sht->bxsize - 21 <= x && x < sht->bxsize - 5 && 5 <= y && y < 19) {
											/* �u�~�v�{�^���N���b�N */
											if ((sht->flags & 0x10) != 0) {		/* �A�v����������E�B���h�E���H */
												task = sht->task;
												cons_putstr0(task->cons, "\nBreak(mouse) :\n");
												io_cli();	/* �����I���������Ƀ^�X�N���ς��ƍ��邩�� */
												task->tss.eax = (int) &(task->tss.esp0);
												task->tss.eip = (int) asm_end_app;
												io_sti();
												task_run(task, -1, 0);
											} else {	/* �R���\�[�� */
												task = sht->task;
												sheet_updown(sht, -1); /* �Ƃ肠������\���ɂ��Ă��� */
												keywin_off(key_win);
												key_win = shtctl->sheets[shtctl->top - 1];
												keywin_on(key_win);
												io_cli();
												fifo32_put(&task->fifo, 4);
												io_sti();
											}
										}
										break;
									}
								}
							}
						} else {
							/* �E�B���h�E�ړ����[�h�̏ꍇ */
							x = mx - mmx;	/* �}�E�X�̈ړ��ʂ��v�Z */
							y = my - mmy;
							new_wx = (mmx2 + x + 2) & ~3;
							new_wy = new_wy + y;
							mmy = my;	/* �ړ���̍��W�ɍX�V */
						}
					} else {
						/* ���{�^���������Ă��Ȃ� */
						mmx = -1;	/* �ʏ탂�[�h�� */
						if (new_wx != 0x7fffffff) {
							sheet_slide(sht, new_wx, new_wy);	/* ��x�m�肳���� */
							new_wx = 0x7fffffff;
						}
					}
				}
			} else if (768 <= i && i <= 1023) {	/* R\[I */
				close_console(shtctl->sheets0 + (i - 768));
			} else if (1024 <= i && i <= 2023) {
				close_constask(taskctl->tasks0 + (i - 1024));
			} else if (2024 <= i && i <= 2279) {	/* R\[ */
				sht2 = shtctl->sheets0 + (i - 2024);
				memman_free_4k(memman, (int) sht2->buf, 256 * 165);
				sheet_free(sht2);
			}
		}
	}
}

void keywin_off(struct SHEET *key_win)
{
	change_wtitle8(key_win, 0);
	if ((key_win->flags & 0x20) != 0) {
		fifo32_put(&key_win->task->fifo, 3); /* �R���\�[���̃J�[�\��OFF */
	}
	return;
}

void keywin_on(struct SHEET *key_win)
{
	change_wtitle8(key_win, 1);
	if ((key_win->flags & 0x20) != 0) {
		fifo32_put(&key_win->task->fifo, 2); /* �R���\�[���̃J�[�\��ON */
	}
	return;
}

struct TASK *open_constask(struct SHEET *sht, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_alloc();
	int *cons_fifo = (int *) memman_alloc_4k(memman, 128 * 4);
	task->cons_stack = memman_alloc_4k(memman, 64 * 1024);
	task->tss.esp = task->cons_stack + 64 * 1024 - 12;
	task->tss.eip = (int) &console_task;
	task->tss.es = 1 * 8;
	task->tss.cs = 2 * 8;
	task->tss.ss = 1 * 8;
	task->tss.ds = 1 * 8;
	task->tss.fs = 1 * 8;
	task->tss.gs = 1 * 8;
	*((int *) (task->tss.esp + 4)) = (int) sht;
	*((int *) (task->tss.esp + 8)) = memtotal;
	task_run(task, 2, 2); /* level=2, priority=2 */
	fifo32_init(&task->fifo, 128, cons_fifo, task);
	return task;
}

struct SHEET *open_console(struct SHTCTL *shtctl, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHEET *sht = sheet_alloc(shtctl);
	unsigned char *buf = (unsigned char *) memman_alloc_4k(memman, 256 * 165);
	sheet_setbuf(sht, buf, 256, 165, -1); /* �����F�Ȃ� */
	make_window8(buf, 256, 165, "console", 0);
	make_textbox8(sht, 8, 28, 240, 128, COL8_000000);
	sht->task = open_constask(sht, memtotal);
	sht->flags |= 0x20;	/* �J�[�\������ */
	return sht;
}

void close_constask(struct TASK *task)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	task_sleep(task);
	memman_free_4k(memman, task->cons_stack, 64 * 1024);
	memman_free_4k(memman, (int) task->fifo.buf, 128 * 4);
	task->flags = 0; /* task_free(task); �̑��� */
	return;
}

void close_console(struct SHEET *sht)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = sht->task;
	memman_free_4k(memman, (int) sht->buf, 256 * 165);
	sheet_free(sht);
	close_constask(task);
	return;
}

void verify_paging(struct SHEET *sht)
{
    struct TASK *task = sht->task;
    struct CONSOLE *cons = task->cons;
    unsigned int cr0_val, cr3_val;
    char s[60];
    unsigned int high_idx, pt_addr, pt_index, mapped_addr;
    unsigned int *pde = (unsigned int *)KERNEL_PAGE_DIR;
    unsigned int *pt;
	int i;

    cmd_cls(cons);
    cons_putstr0(cons, "===== PAGING TEST RESULTS =====\n");

	// 1. check config
    cr0_val = load_cr0();
	cr3_val = read_cr3();
    sprintf(s, "Paging: %s, PD: 0x%08X", (cr0_val & 0x80000000) ? "ON" : "OFF", cr3_val);
    cons_putstr0(cons, s);
    cons_newline(cons);

	// 2. check Page tables
	high_idx = KERNEL_VIRT_ADDR >> 22;
    
    if ((pde[high_idx] & 0x01) == 0) {
        cons_putstr0(cons, "Error: High address page table not present!\n");
        return;
    }

    pt_addr = pde[high_idx] & 0xFFFFF000;
    pt = (unsigned int *)pt_addr;
    pt_index = (0x00280000 >> 12) & 0x3FF;
    mapped_addr = pt[pt_index] & 0xFFFFF000;
    
    sprintf(s, "PDE[%d]=0x%08X -> PT=0x%08X -> Maps to 0x%08X", 
            high_idx, pde[high_idx], pt_addr, mapped_addr);
    cons_putstr0(cons, s);
    cons_newline(cons);

	// 3. test memory access
    unsigned char *test_addr_phys = (unsigned char*)0x00280000;
    unsigned char *test_addr_virt = (unsigned char*)0xC0280000;
    
	// save
    unsigned char backup[8];
    for (i = 0; i < 8; i++) {
        backup[i] = test_addr_phys[i];
    }

    // write phy -> read virt
    cons_putstr0(cons, "Test 1: \n");
    for (i = 0; i < 4; i++) {
        test_addr_phys[i] = 0xA0 + i;
    }
    int test1_pass = 1;
    for (i = 0; i < 4; i++) {
        if (test_addr_virt[i] != (0xA0 + i)) {
            test1_pass = 0;
            break;
        }
    }
    sprintf(s, "Result: %s", test1_pass ? "PASSED" : "FAILED");
    cons_putstr0(cons, s);
    cons_newline(cons);
	
	// write virt -> read phy
    cons_putstr0(cons, "Test 2: \n");
    for (i = 0; i < 4; i++) {
        test_addr_virt[i] = 0xB0 + i;
    }
    int test2_pass = 1;
    for (i = 0; i < 4; i++) {
        if (test_addr_phys[i] != (0xB0 + i)) {
            test2_pass = 0;
            break;
        }
    }
    sprintf(s, "Result: %s", test2_pass ? "PASSED" : "FAILED");
    cons_putstr0(cons, s);
    cons_newline(cons);
	
	// restore
    for (i = 0; i < 8; i++) {
        test_addr_phys[i] = backup[i];
    }
    
    cons_newline(cons);
    if ((cr0_val & 0x80000000) && (cr3_val == KERNEL_PAGE_DIR) && 
        (mapped_addr == 0x00280000) && test1_pass && test2_pass) {
        cons_putstr0(cons, "HIGH MEMORY MAPPING is working correctly!\n");
    }
	else{
        cons_putstr0(cons, "OH NO...something is wrong with paging");
    }
}