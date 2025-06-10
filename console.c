/* 命令行窗口相关 */

#include "bootpack.h"
#include <stdio.h>
#include <string.h>

// 定义全局内存剪切板
struct {
    char filename[12];      // 文件名
    char content[1024];     // 文件内容（最大1KB）
    int size;               // 文件大小
    int valid;              // 剪切板是否有有效内容
} clipboard;

void console_task(struct SHEET *sheet, int memtotal)
{
	struct TASK *task = task_now();
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	int i, *fat = (int *) memman_alloc_4k(memman, 4 * 2880);

	// 使用mem_disk中的FAT表
	int *mfat = mem_disk.fat;
	struct MEM_DISK* disk = get_memory_disk();


	struct FILEHANDLE fhandle[8];
	struct CONSOLE cons;
	char cmdline[30];
	unsigned char *nihongo = (char *) *((int *) 0x0fe8);

	cons.sht = sheet;
	cons.cur_x =  8;
	cons.cur_y = 28;
	cons.cur_c = -1;
	task->cons = &cons;
	task->cmdline = cmdline;

	// // 增 - 初始目录栈
	// task->dir_level = 0;
    // task->current_dir = NULL; // 根目录

	if (cons.sht != 0) {
		cons.timer = timer_alloc();
		timer_init(cons.timer, &task->fifo, 1);
		timer_settime(cons.timer, 50);
	}

	// 兼容加载软盘
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));
	for (i = 0; i < 8; i++) {
		fhandle[i].buf = 0; /*未使用标记*/
	}
	task->fhandle = fhandle;
	task->fat = mfat; // 更改为mfat

	if (nihongo[4096] != 0xff) {	 /* 是否载入了字库？*/
		task->langmode = 1;
	} else {
		task->langmode = 0;
	}
	task->langbyte1 = 0;

	/*显示提示符*/
	cons_putchar(&cons, '>', 1);

	for (;;) {
		io_cli();
		if (fifo32_status(&task->fifo) == 0) {
			task_sleep(task);
			io_sti();
		} else {
			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1 && cons.sht != 0) { /*光标用定时器*/
				if (i != 0) {
					timer_init(cons.timer, &task->fifo, 0); /*下次置0 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_FFFFFF;
					}
				} else {
					timer_init(cons.timer, &task->fifo, 1); /*下次置1 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_000000;
					}
				}
				timer_settime(cons.timer, 50);
			}
			if (i == 2) { /*光标ON */
				cons.cur_c = COL8_FFFFFF;
			}
			if (i == 3) { /*光标OFF */
				if (cons.sht != 0) {
					boxfill8(cons.sht->buf, cons.sht->bxsize, COL8_000000,
						cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				}
				cons.cur_c = -1;
			}
			if (i == 4) { /*点击命令行窗口的“×”按钮*/ 
				cmd_exit(&cons, fat);
			}
			if (256 <= i && i <= 511) { /*键盘数据（通过任务A）*/
				if (i == 8 + 256) {
					/*退格键*/
					if (cons.cur_x > 16) {
					/*用空格擦除光标后将光标前移一位*/
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					}
				} else if (i == 10 + 256) {
					/*回车键*/
					/*将光标用空格擦除后换行 */
					cons_putchar(&cons, ' ', 0);
					cmdline[cons.cur_x / 8 - 2] = 0;
					cons_newline(&cons);


					cons_runcmd(cmdline, &cons, mfat, memtotal);  /*运行命令 - 参数为mfat */ 


					if (cons.sht == 0) {
						cmd_exit(&cons, fat);
					}
					/*显示提示符*/
					cons_putchar(&cons, '>', 1);
				} else {
					/*一般字符*/
					if (cons.cur_x < 240) {
						/*显示一个字符之后将光标后移一位*/
						cmdline[cons.cur_x / 8 - 2] = i - 256;
						cons_putchar(&cons, i - 256, 1);
					}
				}
			}
			/*重新显示光标*/
			if (cons.sht != 0) {
				if (cons.cur_c >= 0) {
					boxfill8(cons.sht->buf, cons.sht->bxsize, cons.cur_c, 
						cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				}
				sheet_refresh(cons.sht, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
			}
		}
	}
}



void cons_putchar(struct CONSOLE *cons, int chr, char move)
{
	char s[2];
	s[0] = chr;
	s[1] = 0;
	if (s[0] == 0x09) { /*制表符*/
		for (;;) {
			if (cons->sht != 0) {
				putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			}
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {
				cons_newline(cons);
			}
			if (((cons->cur_x - 8) & 0x1f) == 0) {
				break; /*被32整除则break*/
			}
		}
	} else if (s[0] == 0x0a) { /*换行*/
		cons_newline(cons);
	} else if (s[0] == 0x0d) { /*回车*/
		/*先不做任何操作*/
	} else { /*一般字符*/
		if (cons->sht != 0) {
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		}
		if (move != 0) {
			/* move为0时光标不后移*/
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {
				cons_newline(cons);
			}
		}
	}
	return;
}

void cons_newline(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	struct TASK *task = task_now();
	if (cons->cur_y < 28 + 112) {
		cons->cur_y += 16; /*到下一行*/
	} else {
		/*滚动*/
		if (sheet != 0) {
			for (y = 28; y < 28 + 112; y++) {
				for (x = 8; x < 8 + 240; x++) {
					sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
				}
			}
			for (y = 28 + 112; y < 28 + 128; y++) {
				for (x = 8; x < 8 + 240; x++) {
					sheet->buf[x + y * sheet->bxsize] = COL8_000000;
				}
			}
			sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
		}
	}
	cons->cur_x = 8;
	if (task->langmode == 1 && task->langbyte1 != 0) {
		cons->cur_x += 8;
	}
	return;
}

void cons_putstr0(struct CONSOLE *cons, char *s)
{
	for (; *s != 0; s++) {
		cons_putchar(cons, *s, 1);
	}
	return;
}

void cons_putstr1(struct CONSOLE *cons, char *s, int l)
{
	int i;
	for (i = 0; i < l; i++) {
		cons_putchar(cons, s[i], 1);
	}
	return;
}


// 注册命令
void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, int memtotal)
{
	if (strcmp(cmdline, "mem") == 0 && cons->sht != 0) {
		cmd_mem(cons, memtotal);
	} else if (strcmp(cmdline, "cls") == 0 && cons->sht != 0) {
		cmd_cls(cons);
	} else if ((strcmp(cmdline, "dir") == 0 || strcmp(cmdline, "ls") == 0) && cons->sht != 0) {
		cmd_dir(cons);
	} else if (strcmp(cmdline, "exit") == 0) {
		cmd_exit(cons, fat);
	} else if (strncmp(cmdline, "start ", 6) == 0) {
		cmd_start(cons, cmdline, memtotal);
	} else if (strncmp(cmdline, "ncst ", 5) == 0) {
		cmd_ncst(cons, cmdline, memtotal);
	} else if (strncmp(cmdline, "langmode ", 9) == 0) {
		cmd_langmode(cons, cmdline);
	}

	// 新增内存文件系统命令
	else if (strcmp(cmdline, "sp") == 0) {
        cmd_sp(cons);
    }
    else if (strncmp(cmdline, "mkdir ", 6) == 0) {
        cmd_mkdir(cons, cmdline + 6);
    } else if (strncmp(cmdline, "touch ", 6) == 0) {
        cmd_touch(cons, cmdline + 6);
    } else if (strncmp(cmdline, "cp ", 3) == 0) {
        // 单参数版本（复制到剪切板）
        cmd_cp(cons, cmdline + 3);
    } else if (strncmp(cmdline, "rm ", 3) == 0) {
        cmd_rm(cons, cmdline + 3);
    } else if (strcmp(cmdline, "df") == 0) {
        cmd_df(cons);
    } else if (strncmp(cmdline, "cat ", 4) == 0) {
        cmd_cat(cons, cmdline + 4, fat);
    } 
	else if (strncmp(cmdline, "write ", 6) == 0) {
        char* filename = cmdline + 6;
        while (*filename && *filename != ' ') filename++;
        if (*filename) {
            *filename = 0;
            cmd_write(cons, cmdline + 6, filename + 1, fat);
        } else {
            cons_putstr0(cons, "write: missing content operand\n");
        }
    }
	else if (strncmp(cmdline, "rmdir ", 6) == 0) {
        cmd_rmdir(cons, cmdline + 6);
    }
	else if (strcmp(cmdline, "lsm") == 0) {
    	cmd_lsm(cons);
    } else if (strncmp(cmdline, "cd ", 3) == 0) {
        cmd_cd(cons, cmdline + 3);
    }
	else if (strcmp(cmdline, "sd") == 0) {
        cmd_sd(cons);
    }
    // 新增粘贴命令
    else if (strcmp(cmdline, "paste") == 0) {
        cmd_paste(cons);
    }
	else if (cmdline[0] != 0) {
		if (cmd_app(cons, fat, cmdline) == 0) {
			/*不是命令，不是应用程序，也不是空行*/
			cons_putstr0(cons, "Bad command.\n\n");
		}
	}
	return;
}

void cmd_mem(struct CONSOLE *cons, int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	char s[60];
	sprintf(s, "total   %dMB\nfree %dKB\n\n", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	cons_putstr0(cons, s);
	return;
}

void cmd_cls(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	for (y = 28; y < 28 + 128; y++) {
		for (x = 8; x < 8 + 240; x++) {
			sheet->buf[x + y * sheet->bxsize] = COL8_000000;
		}
	}
	sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
	cons->cur_y = 28;
	return;
}

void cmd_dir(struct CONSOLE *cons)
{
	struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	int i, j;
	char s[30];
	for (i = 0; i < 224; i++) {
		if (finfo[i].name[0] == 0x00) {
			break;
		}
		if (finfo[i].name[0] != 0xe5) {
			if ((finfo[i].type & 0x18) == 0) {
				sprintf(s, "filename.ext   %7d\n", finfo[i].size);
				for (j = 0; j < 8; j++) {
					s[j] = finfo[i].name[j];
				}
				s[ 9] = finfo[i].ext[0];
				s[10] = finfo[i].ext[1];
				s[11] = finfo[i].ext[2];
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}

void cmd_exit(struct CONSOLE *cons, int *fat)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_now();
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct FIFO32 *fifo = (struct FIFO32 *) *((int *) 0x0fec);
	if (cons->sht != 0) {
		timer_cancel(cons->timer);
	}
	memman_free_4k(memman, (int) fat, 4 * 2880);
	io_cli();
	if (cons->sht != 0) {
		fifo32_put(fifo, cons->sht - shtctl->sheets0 + 768); /* 768〜1023 */
	} else {
		fifo32_put(fifo, task - taskctl->tasks0 + 1024); /*1024～2023*/
	}
	io_sti();
	for (;;) {
		task_sleep(task);
	}
}

void cmd_start(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht = open_console(shtctl, memtotal);
	struct FIFO32 *fifo = &sht->task->fifo;
	int i;
	sheet_slide(sht, 32, 4);
	sheet_updown(sht, shtctl->top);
	/*将命令行输入的字符串逐字复制到新的命令行窗口中*/
	for (i = 6; cmdline[i] != 0; i++) {
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256);	 /*回车键*/
	cons_newline(cons);
	return;
}

void cmd_ncst(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct TASK *task = open_constask(0, memtotal);
	struct FIFO32 *fifo = &task->fifo;
	int i;

	/*将命令行输入的字符串逐字复制到新的命令行窗口中*/
	for (i = 5; cmdline[i] != 0; i++) {
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256); /*回车键*/
	cons_newline(cons);
	return;
}

void cmd_langmode(struct CONSOLE *cons, char *cmdline)
{
	struct TASK *task = task_now();
	unsigned char mode = cmdline[9] - '0';
	if (mode <= 2) {
		task->langmode = mode;
	} else {
		cons_putstr0(cons, "mode number error.\n");
	}
	cons_newline(cons);
	return;
}

int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo;
	char name[18], *p, *q;
	struct TASK *task = task_now();
	int i, segsiz, datsiz, esp, dathrb, appsiz;
	struct SHTCTL *shtctl;
	struct SHEET *sht;

	/*根据命令行生成文件名*/
	for (i = 0; i < 13; i++) {
		if (cmdline[i] <= ' ') {
			break;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0; /*暂且将文件名的后面置为0*/

	/*寻找文件 */
	finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo == 0 && name[i - 1] != '.') {
		/*由于找不到文件，故在文件名后面加上“.hrb”后重新寻找*/
		name[i    ] = '.';
		name[i + 1] = 'H';
		name[i + 2] = 'R';
		name[i + 3] = 'B';
		name[i + 4] = 0;
		finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	}

	if (finfo != 0) {
		/*如果找到文件*/
		appsiz = finfo->size;
		p = file_loadfile2(finfo->clustno, &appsiz, fat);
		if (appsiz >= 36 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00) {
			segsiz = *((int *) (p + 0x0000));
			esp    = *((int *) (p + 0x000c));
			datsiz = *((int *) (p + 0x0010));
			dathrb = *((int *) (p + 0x0014));
			q = (char *) memman_alloc_4k(memman, segsiz);
			task->ds_base = (int) q;
			set_segmdesc(task->ldt + 0, finfo->size - 1, (int) p, AR_CODE32_ER + 0x60);
			set_segmdesc(task->ldt + 1, segsiz - 1, (int) q, AR_DATA32_RW + 0x60); 
			for (i = 0; i < datsiz; i++) {
				q[esp + i] = p[dathrb + i];
			}
			start_app(0x1b, 0 * 8 + 4, esp, 1 * 8 + 4, &(task->tss.esp0));
			shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
			for (i = 0; i < MAX_SHEETS; i++) {
				sht = &(shtctl->sheets0[i]);
				if ((sht->flags & 0x11) == 0x11 && sht->task == task) {
					/*找到被应用程序遗留的窗口*/
					sheet_free(sht); /*关闭*/
				}
			}
			for (i = 0; i < 8; i++) { /*将未关闭的文件关闭*/ 
				if (task->fhandle[i].buf != 0) {
					memman_free_4k(memman, (int) task->fhandle[i].buf, task->fhandle[i].size);
					task->fhandle[i].buf = 0;
				}
			}
			timer_cancelall(&task->fifo);
			memman_free_4k(memman, (int) q, segsiz);
			task->langbyte1 = 0;
		} else {
			cons_putstr0(cons, ".hrb file format error.\n");
		}
		memman_free_4k(memman, (int) p, appsiz);
		cons_newline(cons);
		return 1;
	}
	/*没有找到文件的情况*/
	return 0;
}

int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
	struct TASK *task = task_now();
	int ds_base = task->ds_base;
	struct CONSOLE *cons = task->cons;
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht;
	struct FIFO32 *sys_fifo = (struct FIFO32 *) *((int *) 0x0fec);
	int *reg = &eax + 1; /* eax后面的地址*/
	/*强行改写通过PUSHAD保存的值*/
		/* reg[0] : EDI,   reg[1] : ESI,   reg[2] : EBP,   reg[3] : ESP */
		/* reg[4] : EBX,   reg[5] : EDX,   reg[6] : ECX,   reg[7] : EAX */
	int i;
	struct FILEINFO *finfo;
	struct FILEHANDLE *fh;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;

	if (edx == 1) {
		cons_putchar(cons, eax & 0xff, 1);
	} else if (edx == 2) {
		cons_putstr0(cons, (char *) ebx + ds_base);
	} else if (edx == 3) {
		cons_putstr1(cons, (char *) ebx + ds_base, ecx);
	} else if (edx == 4) {
		return &(task->tss.esp0);
	} else if (edx == 5) {
		sht = sheet_alloc(shtctl);
		sht->task = task;
		sht->flags |= 0x10;
		sheet_setbuf(sht, (char *) ebx + ds_base, esi, edi, eax);
		make_window8((char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
		sheet_slide(sht, ((shtctl->xsize - esi) / 2) & ~3, (shtctl->ysize - edi) / 2);
		sheet_updown(sht, shtctl->top); /*将窗口图层高度指定为当前鼠标所在图层的高度，鼠标移到上层*/
		reg[7] = (int) sht;
	} else if (edx == 6) {
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
		}
	} else if (edx == 7) {
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	} else if (edx == 8) {
		memman_init((struct MEMMAN *) (ebx + ds_base));
		ecx &= 0xfffffff0; /*以16字节为单位*/
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	} else if (edx == 9) {
		ecx = (ecx + 0x0f) & 0xfffffff0; /*以16字节为单位进位取整*/
		reg[7] = memman_alloc((struct MEMMAN *) (ebx + ds_base), ecx);
	} else if (edx == 10) {
		ecx = (ecx + 0x0f) & 0xfffffff0; /*以16字节为单位进位取整*/
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	} else if (edx == 11) {
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		sht->buf[sht->bxsize * edi + esi] = eax;
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
		}
	} else if (edx == 12) {
		sht = (struct SHEET *) ebx;
		sheet_refresh(sht, eax, ecx, esi, edi);
	} else if (edx == 13) {
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		hrb_api_linewin(sht, eax, ecx, esi, edi, ebp);
		if ((ebx & 1) == 0) {
			if (eax > esi) {
				i = eax;
				eax = esi;
				esi = i;
			}
			if (ecx > edi) {
				i = ecx;
				ecx = edi;
				edi = i;
			}
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	} else if (edx == 14) {
		sheet_free((struct SHEET *) ebx);
	} else if (edx == 15) {
		for (;;) {
			io_cli();
			if (fifo32_status(&task->fifo) == 0) {
				if (eax != 0) {
					task_sleep(task); /* FIFO为空，休眠并等待*/
				} else {
					io_sti();
					reg[7] = -1;
					return 0;
				}
			}
			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1) { /*光标用定时器*/
				/*应用程序运行时不需要显示光标，因此总是将下次显示用的值置为1*/
				timer_init(cons->timer, &task->fifo, 1); /*下次置为1*/
				timer_settime(cons->timer, 50);
			}
			if (i == 2) { /*光标ON */
				cons->cur_c = COL8_FFFFFF;
			}
			if (i == 3) { /*光标OFF */
				cons->cur_c = -1;
			}
			if (i == 4) { /*只关闭命令行窗口*/
				timer_cancel(cons->timer);
				io_cli();
				fifo32_put(sys_fifo, cons->sht - shtctl->sheets0 + 2024); /*2024～2279*/
				cons->sht = 0;
				io_sti();
			}
			if (i >= 256) { /*键盘数据（通过任务A）等*/
				reg[7] = i - 256;
				return 0;
			}
		}
	} else if (edx == 16) {
		reg[7] = (int) timer_alloc();
		((struct TIMER *) reg[7])->flags2 = 1; /*允许自动取消*/
	} else if (edx == 17) {
		timer_init((struct TIMER *) ebx, &task->fifo, eax + 256);
	} else if (edx == 18) {
		timer_settime((struct TIMER *) ebx, eax);
	} else if (edx == 19) {
		timer_free((struct TIMER *) ebx);
	} else if (edx == 20) {
		if (eax == 0) {
			i = io_in8(0x61);
			io_out8(0x61, i & 0x0d);
		} else {
			i = 1193180000 / eax;
			io_out8(0x43, 0xb6);
			io_out8(0x42, i & 0xff);
			io_out8(0x42, i >> 8);
			i = io_in8(0x61);
			io_out8(0x61, (i | 0x03) & 0x0f);
		}
	} else if (edx == 21) {
		for (i = 0; i < 8; i++) {
			if (task->fhandle[i].buf == 0) {
				break;
			}
		}
		fh = &task->fhandle[i];
		reg[7] = 0;
		if (i < 8) {
			finfo = file_search((char *) ebx + ds_base,
					(struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
			if (finfo != 0) {
				reg[7] = (int) fh;
				fh->size = finfo->size;
				fh->pos = 0;
				fh->buf = file_loadfile2(finfo->clustno, &fh->size, task->fat);
			}
		}
	} else if (edx == 22) {
		fh = (struct FILEHANDLE *) eax;
		memman_free_4k(memman, (int) fh->buf, fh->size);
		fh->buf = 0;
	} else if (edx == 23) {
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0) {
			fh->pos = ebx;
		} else if (ecx == 1) {
			fh->pos += ebx;
		} else if (ecx == 2) {
			fh->pos = fh->size + ebx;
		}
		if (fh->pos < 0) {
			fh->pos = 0;
		}
		if (fh->pos > fh->size) {
			fh->pos = fh->size;
		}
	} else if (edx == 24) {
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0) {
			reg[7] = fh->size;
		} else if (ecx == 1) {
			reg[7] = fh->pos;
		} else if (ecx == 2) {
			reg[7] = fh->pos - fh->size;
		}
	} else if (edx == 25) {
		fh = (struct FILEHANDLE *) eax;
		for (i = 0; i < ecx; i++) {
			if (fh->pos == fh->size) {
				break;
			}
			*((char *) ebx + ds_base + i) = fh->buf[fh->pos];
			fh->pos++;
		}
		reg[7] = i;
	} else if (edx == 26) {
		i = 0;
		for (;;) {
			*((char *) ebx + ds_base + i) =  task->cmdline[i];
			if (task->cmdline[i] == 0) {
				break;
			}
			if (i >= ecx) {
				break;
			}
			i++;
		}
		reg[7] = i;
	} else if (edx == 27) {
		reg[7] = task->langmode;
	}
	return 0;
}

int *inthandler0c(int *esp)
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0); /*强制结束程序*/
}

int *inthandler0d(int *esp)
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0);	/*强制结束程序*/
}

void hrb_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col)
{
	int i, x, y, len, dx, dy;

	dx = x1 - x0;
	dy = y1 - y0;
	x = x0 << 10;
	y = y0 << 10;
	if (dx < 0) {
		dx = - dx;
	}
	if (dy < 0) {
		dy = - dy;
	}
	if (dx >= dy) {
		len = dx + 1;
		if (x0 > x1) {
			dx = -1024;
		} else {
			dx =  1024;
		}
		if (y0 <= y1) {
			dy = ((y1 - y0 + 1) << 10) / len;
		} else {
			dy = ((y1 - y0 - 1) << 10) / len;
		}
	} else {
		len = dy + 1;
		if (y0 > y1) {
			dy = -1024;
		} else {
			dy =  1024;
		}
		if (x0 <= x1) {
			dx = ((x1 - x0 + 1) << 10) / len;
		} else {
			dx = ((x1 - x0 - 1) << 10) / len;
		}
	}

	for (i = 0; i < len; i++) {
		sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
		x += dx;
		y += dy;
	}

	return;
}

/////////
// 以下为内存磁盘 FAT12文件系统 新增命令处理

// 在当期那工作目录下创建目录
void cmd_mkdir(struct CONSOLE* cons, char* dirname) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    struct FILEINFO *parent_dir = disk->current_dir ? disk->current_dir : disk->root_dir;
    
    // 检查目录名是否已存在
    struct FILEINFO* existing = find_file_info(parent_dir, dirname);
    if (existing) {
        cons_putstr0(cons, "Directory already exists.\n");
        return;
    }
    
    // 创建新目录
    struct FILEINFO* finfo = create_directory_in_memory(parent_dir, dirname);
    if (finfo) {
        cons_putstr0(cons, "Directory created.\n");
    } else {
        cons_putstr0(cons, "Failed to create directory.\n");
    }
}

// 创建文件
void cmd_touch(struct CONSOLE* cons, char* filename) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    struct FILEINFO *current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;

    // 在当前目录查找文件 - 已二次定位
    struct FILEINFO* finfo = find_file_info(current_dir, filename);
    if (finfo) {
        cons_putstr0(cons, "File already exists.\n");
        return;
    }

    // 文件不存在，在当前目录创建新文件
    finfo = create_file_in_memory(current_dir, filename, 0);
    if (!finfo) {
        cons_putstr0(cons, "Failed to create file.\n");
        return;
    }

    cons_putstr0(cons, "File created successfully.\n");
}

// 复制文件
void cmd_cp(struct CONSOLE* cons, char* filename) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    struct FILEINFO *current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;

    // 在当前目录查找文件 - 二次定位
    struct FILEINFO* finfo = find_file_info(current_dir, filename);
    if (!finfo) {
        cons_putstr0(cons, "File not found.\n");
        return;
    }

    // 读取文件内容到剪切板
    if (finfo->size > sizeof(clipboard.content)) {
        cons_putstr0(cons, "File too large to copy.\n");
        return;
    }

    file_loadfile_from_memory(finfo->clustno, finfo->size, clipboard.content, disk->fat);
    strncpy(clipboard.filename, filename, 11);
    clipboard.size = finfo->size;
    clipboard.valid = 1;

    cons_putstr0(cons, "File copied to clipboard.\n");
}

// 从剪切板粘贴文件到当前目录
void cmd_paste(struct CONSOLE* cons) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    // 检查剪切板是否有内容
    if (!clipboard.valid) {
        cons_putstr0(cons, "Clipboard is empty.\n");
        return;
    }

    struct FILEINFO *current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;

    // 在当前目录查找是否已存在同名文件
    struct FILEINFO* finfo = find_file_info(current_dir, clipboard.filename);
    if (finfo) {
        cons_putstr0(cons, "File already exists. Overwriting...\n");
        // 更新文件大小
        finfo->size = clipboard.size;
    } else {
        // 文件不存在，在当前目录创建新文件
        finfo = create_file_in_memory(current_dir, clipboard.filename, clipboard.size);
        if (!finfo) {
            cons_putstr0(cons, "Failed to create file.\n");
            return;
        }
    }

    // 统一写入文件内容
    int result = file_writefile_to_memory(finfo->clustno, clipboard.size, clipboard.content, disk->fat);
    if (result < 0) {
        cons_putstr0(cons, "Failed to write file.\n");
    } else {
        cons_putstr0(cons, "File pasted successfully.\n");
    }
}

// 删除文件
void cmd_rm(struct CONSOLE* cons, char* filename) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    struct FILEINFO *current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;

    // 在当前目录查找文件
    struct FILEINFO* finfo = find_file_info(current_dir, filename);
    if (!finfo) {
        cons_putstr0(cons, "File not found.\n");
        return;
    }

    // 检查是否为目录
    if ((finfo->type & 0x10) == 0x10) {
        cons_putstr0(cons, "Use rmdir instead.\n");
        return;
    }

	// 释放文件占用的簇
    int clustno = finfo->clustno;
    while (clustno != 0xfff) {
        int next_clust = disk->fat[clustno];
        disk->fat[clustno] = 0; // 标记为空闲
        clustno = next_clust;
    }

    // 标记文件为删除
    finfo->name[0] |= 0xe5;
    cons_putstr0(cons, "File deleted.\n");
}

// 查找文件信息 - 应该从当前工作目录该开始查找 - 目前只支持单簇目录（根目录224，非根目录16-2=14）
// 查找文件信息 - 从当前工作目录开始查找（单簇目录）
// 分别处理根目录和非根目录的情况
struct FILEINFO* find_file_info(struct FILEINFO* current_dir, const char* name) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return 0;
    }

    char short_name[11];
    parse_short_name(name, short_name);

    struct FILEINFO* search_dir;
    int max_entries;
    int start_index = 1;

    if (current_dir == disk->root_dir) {
        // 根目录情况
        search_dir = current_dir;
        max_entries = 224;
    } else {
        // 非根目录情况
        int data_start = FAT12_DATA_START / 512;
        int clustno = current_dir->clustno;
        search_dir = (struct FILEINFO*)&disk->data[(clustno + data_start) * 512];
        max_entries = 16;  // 单簇目录先限制为 16 个条目
        start_index = 2;   // 跳过 . 和 .. 条目
    }

    int i;
    for (i = start_index; i < max_entries; i++) {
        if (search_dir[i].name[0] == 0x00) break; // 空条目结束
        if ((search_dir[i].name[0] & 0xe5) == 0xe5) {
            continue; // 已删除的文件项
        }

        if (strncmp(search_dir[i].name, short_name, 8) == 0 && strncmp(search_dir[i].ext, short_name + 8, 3) == 0) {
            return &search_dir[i];
        }
    }

    if (i >= max_entries) {
        // 超出单簇目录限制，可根据需求添加更详细的错误处理
        // 这里简单返回 0 表示未找到
        return 0;
    }

    return 0;
}
/*
因为传入的参数是当前工作的目录,那么当工作目录时根目录时，可以直接遍历224个entry来查询
但是如果当工作目录是非根目录时，不能直接遍历，应该先找到current_dir真正存放它目录文件的地方的地址，那里才是current真正存放它目录下entry的地方，因此你应该先current_dir->clusterno，然后计算出
(clusterno + 数据区起始/512)*512找到地址，再遍历16个底下的目录项（直接从第3个开始，因为前面有.与..两项了，先限制单簇目录，所以先遍历到16，如果超16，则提示报错）
*/

// 显示磁盘信息
void cmd_df(struct CONSOLE* cons) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }
    
    int total_clusters = MAX_CLUSTERS;
    int used_clusters = 0;
    int i;
    
    for (i = 3; i < MAX_CLUSTERS; i++) {
        if (disk->fat[i] != 0) {
            used_clusters++;
        }
    }
    
    char s[100];
    sprintf(s, "Total space: %d KB\nUsed space: %d KB\nFree space: %d KB\n", 
            DISK_SIZE / 1024, used_clusters * 512 / 1024, (total_clusters - used_clusters) * 512 / 1024);
    cons_putstr0(cons, s);
}

// 显示文件内容
void cmd_cat(struct CONSOLE* cons, char* filename, int* fat) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }
    
    struct FILEINFO *current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;
    
    struct FILEINFO* finfo = find_file_info(current_dir, filename);
    if (!finfo) {
        cons_putstr0(cons, "File not found.\n");
        return;
    }
    
    int size = finfo->size;
    char* buf = (char*)memman_alloc_4k((struct MEMMAN*)MEMMAN_ADDR, size);
    if (!buf) {
        cons_putstr0(cons, "Memory allocation failed.\n");
        return;
    }
    
    file_loadfile_from_memory(finfo->clustno, size, buf, fat);
    cons_putstr1(cons, buf, size);
    memman_free_4k((struct MEMMAN*)MEMMAN_ADDR, (int)buf, size);
}

// 写文件
void cmd_write(struct CONSOLE* cons, char* filename, char* content, int* fat) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    struct FILEINFO *current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;

    // 在当前目录查找文件
    struct FILEINFO* finfo = find_file_info(current_dir, filename);
    if (!finfo) {
        // 文件不存在，在当前目录创建新文件
        finfo = create_file_in_memory(current_dir, filename, strlen(content));
        if (!finfo) {
            cons_putstr0(cons, "Failed to create file.\n");
            return;
        }
    } else {
        // 文件已存在，更新文件大小
        finfo->size = strlen(content);
    }

    // 写入文件内容
    int result = file_writefile_to_memory(finfo->clustno, strlen(content), content, disk->fat);
    if (result < 0) {
        cons_putstr0(cons, "Failed to write file.\n");
    } else {
        cons_putstr0(cons, "File written successfully.\n");
    }
}

int is_directory_empty(struct FILEINFO* dir_info, int* fat) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return 0; // 磁盘未初始化，认为目录不为空
    }

    int data_start = FAT12_DATA_START / 512;
    int clustno = dir_info->clustno;

    struct FILEINFO* search_dir;
    int max_entries;
    int start_index = 0; // 默认

    if (dir_info == disk->root_dir) {
        // 根目录情况
        search_dir = dir_info;
        max_entries = 224;
    } else {
        // 非根目录情况
        search_dir = (struct FILEINFO*)&disk->data[(clustno + data_start) * 512];
        max_entries = 16;  // 单簇目录先限制为 16 个条目
        start_index = 2;   // 跳过 . 和 .. 条目
    }
	int i = 0;
    for (i = start_index; i < max_entries; i++) {
        if (search_dir[i].name[0] != 0x00 && search_dir[i].name[0] != 0xe5) {
            return 0; // 目录不为空
        }
    }

    return 1; // 目录为空
}

// 删除目录 - 也要把目录下数据区的 . 与 .. 条目标记为删除
void cmd_rmdir(struct CONSOLE* cons, char* dirname) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    struct FILEINFO *current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;

    struct FILEINFO* finfo = find_file_info(current_dir, dirname);
    if (!finfo) {
        cons_putstr0(cons, "Directory not found.\n");
        return;
    }

    if ((finfo->type & 0x10) == 0) {
        cons_putstr0(cons, "Not a directory.\n");
        return;
    }

    if (!is_directory_empty(finfo, disk->fat)) {
        cons_putstr0(cons, "Directory is not empty.\n");
        return;
    }

	// 释放目录占用的簇
    int clustno = finfo->clustno;
    while (clustno != 0xfff) {
        int next_clust = disk->fat[clustno];
        disk->fat[clustno] = 0; // 标记为空闲
        clustno = next_clust;
    }

    // 标记目录本身为删除
    finfo->name[0] |= 0xe5; 

    // 标记目录下数据区的 . 与 .. 条目标记为删除
    int data_start = FAT12_DATA_START / 512;
    clustno = finfo->clustno;
    struct FILEINFO* dir_entries = (struct FILEINFO*)&disk->data[(clustno + data_start) * 512];
    // 标记 . 条目为删除
    dir_entries[0].name[0] |= 0xe5; 
    // 标记 .. 条目为删除
    dir_entries[1].name[0] |= 0xe5; 

    cons_putstr0(cons, "Directory deleted.\n");
}

// 显示当前目录下的 entry
void cmd_lsm(struct CONSOLE *cons)
{
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    struct FILEINFO *current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;
    if (current_dir == disk->root_dir) {
        cons_putstr0(cons, "Listing root directory.\n");
    }
    
    struct FILEINFO *finfo;
    int max_entries;
    int start_index = 1;

    if (current_dir == disk->root_dir) {
        // 根目录情况
        finfo = current_dir;
        max_entries = 224;
    } else {
        // 非根目录情况
        int data_start = FAT12_DATA_START / 512;
        int clustno = current_dir->clustno;
        finfo = (struct FILEINFO*)&disk->data[(clustno + data_start) * 512];
        max_entries = 16;  // 单簇目录先限制为 16 个条目
        start_index = 2;   // 跳过 . 和 .. 条目
    }

    int i, j;
    char s[30];
    for (i = start_index; i < max_entries; i++) {
        if (finfo[i].name[0] == 0x00) {
            break;  // 空条目结束遍历
        }
        
        // 过滤被删除的文件项 (name[0] == 0xE5)
        if ((finfo[i].name[0] & 0xE5) == 0xE5) {
            continue;
        }
        
        if ((finfo[i].type & 0x10) == 0x10) { // 目录
            // 初始化输出字符串
            memset(s, ' ', sizeof(s));
            s[sizeof(s) - 1] = '\0';
            
            // 复制目录名
            for (j = 0; j < 8; j++) {
                if (finfo[i].name[j] != ' ') {
                    s[j] = finfo[i].name[j];
                }
            }
            
            // 设置扩展名为 DIR
            s[9] = 'D';
            s[10] = 'I';
            s[11] = 'R';
            
            // 格式化输出大小（目录大小为 0）
            sprintf(s + 15, "%7d\n", 0);
        } else { // 文件
            sprintf(s, "filename.ext   %7d\n", finfo[i].size);
            
            for (j = 0; j < 8; j++) {
                s[j] = finfo[i].name[j];
            }
            
            s[9] = finfo[i].ext[0];
            s[10] = finfo[i].ext[1];
            s[11] = finfo[i].ext[2];
        }
        
        cons_putstr0(cons, s);
    }
    
    cons_newline(cons);
    return;
}

// cd 指令 - 修复重复进入同一目录的问题
// find_file_info 会跳过. 与 .. 条目，这样就无法反复进入自己
// find_file_info 会跳过. 与 .. 条目，这样就无法反复进入自己
void cmd_cd(struct CONSOLE* cons, char* dirname) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    // 处理 cd / 返回根目录
    if (strcmp(dirname, "/") == 0) {
        disk->dir_level = 0;
        disk->current_dir = NULL; // 根目录
        // 清空目录栈
        int i = 0;
        for (i = 0; i < 16; i++) {
            disk->dir_stack[i] = NULL;
        }
        cons_putstr0(cons, "Returned to root directory.\n");
        return;
    }
    
    // 处理 cd .. 返回上一级目录
    if (strcmp(dirname, "..") == 0) {
        if (disk->dir_level > 0) {
            disk->dir_stack[disk->dir_level] = NULL; // 释放当前目录
            disk->dir_level--;
            disk->current_dir = disk->dir_stack[disk->dir_level];
            cons_putstr0(cons, "Moved up one level.\n");
        } else {
            cons_putstr0(cons, "Already at root directory.\n");
        }
        return;
    }

    // 处理进入子目录
    struct FILEINFO *root_dir = disk->current_dir ? disk->current_dir : disk->root_dir;
    // 这里 finfo 给的是子目录的entry - find_file_info 已经处理二次定位了
    struct FILEINFO* finfo = find_file_info(root_dir, dirname);
    
    if (!finfo) {
        cons_putstr0(cons, "Directory not found.\n");
        return;
    }

    if ((finfo->type & 0x10) == 0) {
        cons_putstr0(cons, "Not a directory.\n");
        return;
    }

    // 检查是否已经在目标目录中
    if (disk->current_dir == finfo) {
        cons_putstr0(cons, "Already in this directory.\n");
        return;
    }

    // 更新目录栈
    if (disk->dir_level < 15) {
        // 这里的 current_dir 还是父目录
        if (disk->current_dir) {
            disk->dir_stack[disk->dir_level] = disk->current_dir;
        }
        // 更新 current_dir
        disk->dir_level++;
        disk->current_dir = finfo;
        cons_putstr0(cons, "Entered directory.\n");
    } else {
        cons_putstr0(cons, "Directory nesting limit reached.\n");
    }
}

// show_dir: 显示当前所在目录
void cmd_sd(struct CONSOLE* cons) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }
    char path[256] = "/";
    char temp[12];
    int i;
    
    // 构建完整路径
    for (i = 0; i < disk->dir_level; i++) {
        if (disk->dir_stack[i] && disk->dir_stack[i]->name[0] != 0) {
            parse_short_name((const char*)disk->dir_stack[i]->name, temp);
            trim_trailing_spaces(temp); // 移除末尾空格
            strcat(path, temp);
            strcat(path, "/");
        }
    }
    
    // 添加当前目录名
    if (disk->current_dir && disk->current_dir->name[0] != 0) {
        parse_short_name((const char*)disk->current_dir->name, temp);
        trim_trailing_spaces(temp); // 移除末尾空格
        strcat(path, temp);
        strcat(path, "/");
    }
    
    cons_putstr0(cons, path);
    cons_putstr0(cons, "\n");
}

// 移除字符串末尾的空格 - 工具函数
void trim_trailing_spaces(char *str) {
    int len = strlen(str);
    while (len > 0 && str[len - 1] == ' ') {
        str[len - 1] = '\0';
        len--;
    }
}


// show pointer: 显示当前工作目录在内存磁盘中的实际地址
void cmd_sp(struct CONSOLE* cons) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        cons_putstr0(cons, "Memory disk not initialized.\n");
        return;
    }

    struct FILEINFO* current_dir = disk->current_dir ? disk->current_dir : disk->root_dir;
    struct FILEINFO* entry_addr = NULL;
    char ptr_str[20];

    if (current_dir == disk->root_dir) {
        // 根目录：直接使用根目录数组地址
		// 这里的地址是内存中的绝对地址了，而不是内存磁盘的偏移地址
        entry_addr = disk->root_dir;
    } else {
        // 非根目录：根据簇号计算数据区中的地址
        int data_start = FAT12_DATA_START / 512;
        int clustno = current_dir->clustno;
        entry_addr = (struct FILEINFO*)&disk->data[(clustno + data_start) * 512];
    }

    sprintf(ptr_str, "Address: %p\n", (void*)entry_addr);
    cons_putstr0(cons, ptr_str);
}