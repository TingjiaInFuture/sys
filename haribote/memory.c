#include "bootpack.h"
#include <string.h>
#include <stdio.h>

#define EFLAGS_AC_BIT		0x00040000
#define CR0_CACHE_DISABLE	0x60000000

/////////////////
struct MEM_DISK mem_disk;

// 初始化内存磁盘
void init_memory_disk(struct MEMMAN* memman) {
    // 分配磁盘空间
    mem_disk.data = (unsigned char*)memman_alloc_4k(memman, DISK_SIZE);
    mem_disk.fat = (int*)memman_alloc_4k(memman, FAT12_FAT_SIZE); // 仅分配1个FAT表
    mem_disk.root_dir = (struct FILEINFO*)memman_alloc_4k(memman, FAT12_ROOT_DIR_SIZE); // 14个簇 7168 字节会向上对齐到 8KB
    
    if (!mem_disk.data || !mem_disk.fat || !mem_disk.root_dir) {
        return; // 内存分配失败
    }
    
    // 初始化引导扇区
    init_boot_sector(mem_disk.data);
    
    // 初始化FAT表
    init_fat_table(mem_disk.fat);
    
    // 初始化根目录
    init_root_dir(mem_disk.root_dir);

	// 初始化目录信息
    mem_disk.dir_level = 0;
    mem_disk.current_dir = NULL; // 根目录
	int i = 0;
    for (i = 0; i < 16; i++) {
        mem_disk.dir_stack[i] = NULL;
    }
    
    mem_disk.initialized = 1;
}

// 初始化引导扇区
void init_boot_sector(unsigned char* boot_sector) {
    // 填充引导扇区数据，设置BPB参数
    // 这里按照FAT12格式初始化引导扇区
    memset(boot_sector, 0, FAT12_BOOT_SECTOR_SIZE);
    
    // 设置BPB参数
    unsigned short* bpb = (unsigned short*)(boot_sector + 11);
    bpb[0] = 512;       // 每扇区字节数
    bpb[1] = 1;         // 每簇扇区数
    bpb[2] = 1;         // 保留扇区数
    bpb[3] = 2;         // FAT表数量 --- 当前为简化版本 -- 一个FAT表
    bpb[5] = 224;       // 根目录项数
    bpb[6] = 2880;      // 总扇区数(小)
    bpb[7] = 2;         // 媒体描述符
    bpb[8] = 9;         // 每FAT扇区数
    bpb[10] = 0x29;     // 扩展BPB签名
    unsigned int* bpb_unsigned = (unsigned int*)(boot_sector + 32);
    bpb_unsigned[0] = DISK_SIZE / 512; // 总扇区数(大)
    bpb_unsigned[2] = 0x12345678;      // 卷序列号
    
    // 引导代码和结束标志
    boot_sector[510] = 0x55;
    boot_sector[511] = 0xaa;
}

// 初始化FAT表
void init_fat_table(int* fat) {
    int i;
    // 初始化FAT表
    for (i = 0; i < FAT12_FAT_SIZE; i++) {
        fat[i] = 0;
    }
    
    // 标记坏簇和结束簇
    fat[0] = 0xf0ff; // 媒体描述符
    fat[1] = 0xffff; // 保留簇
    fat[2] = 0xffff; // 根目录起始簇
}

// 初始化根目录
void init_root_dir(struct FILEINFO* root_dir) {
    // 清空根目录
    memset(root_dir, 0, FAT12_ROOT_DIR_SIZE);
	// 占用第一个条目（保留项）
	// 第一个entry放在根目录的起始位置，和根目录起始的地址一致，那么会导致其他指令在识别currentdir时识别为根目录，直接不走子目录的处理
    memset(root_dir[0].name, 0, 11);
    root_dir[0].name[0] = 0x05;        // 特殊字符，表示隐藏/系统文件
    root_dir[0].type = 0x08;           // 系统文件属性
    root_dir[0].clustno = 0;           // 无簇
    root_dir[0].size = 0;              // 大小为0

}

<<<<<<< HEAD
void init_paging(void)
{
    unsigned int *pde = (unsigned int *)KERNEL_PAGE_DIR;
    unsigned int *pte = (unsigned int *)KERNEL_PAGE_TBL;
    unsigned int addr, i, j;

	// clear
    for (i = 0; i < 1024; i++) {
        pde[i] = 0;
    }
	for (i = 0; i < 1024 * 1024; i++) {
        pte[i] = 0;
    }

    // identity mapping
    for (i = 0; i < 1024; i++) {
        pde[i] = (KERNEL_PAGE_TBL + i * 4096) | PG_P | PG_RW | PG_US;
        for (j = 0; j < 1024; j++) {
            pte[i * 1024 + j] = ((i * 1024 + j) * 4096) | PG_P | PG_RW | PG_US;
        }
    }

	// high memory mapping
	unsigned int high_pte_offset = 1024 * 1024;

	for (i = 0; i < 16; i++) {
        pde[i + 768] = (KERNEL_PAGE_TBL + (1024 + i) * 4096) | PG_P | PG_RW | PG_US;
        for (j = 0; j < 1024; j++) {
            pte[high_pte_offset + i * 1024 + j] = ((i * 1024 + j) * 4096) | PG_P | PG_RW | PG_US;
        }
    }

    unsigned int pdt_page = KERNEL_PAGE_DIR / 4096;
    unsigned int pde_idx = pdt_page / 1024;
    unsigned int pte_idx = pdt_page % 1024;
    pte[pde_idx * 1024 + pte_idx] = KERNEL_PAGE_DIR | PG_P | PG_RW | PG_US;

	unsigned int pt_start_page = KERNEL_PAGE_TBL / 4096;
    unsigned int pt_end_page = (KERNEL_PAGE_TBL + 1024 * 4096) / 4096;
    for (i = pt_start_page; i < pt_end_page; i++) {
        pde_idx = i / 1024;
        pte_idx = i % 1024;
        pte[pde_idx * 1024 + pte_idx] = (i * 4096) | PG_P | PG_RW | PG_US;
    }
	
    store_cr3(KERNEL_PAGE_DIR);
    flush_tlb();
    unsigned int cr0 = load_cr0();
    cr0 |= CR0_PG;
    store_cr0(cr0);
}

=======
>>>>>>> cd96f5c5205cd987613df74b40457a5ca0d64373
////////////////

unsigned int memtest(unsigned int start, unsigned int end)
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;

	/* 确认CPU是386还是486以上的 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0) {
		/* 如果是386，即使设定AC=1，AC的值还会自动回到0 */
		flg486 = 1;
	}

	eflg &= ~EFLAGS_AC_BIT; /* AC-bit = 0 */
	io_store_eflags(eflg);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; /* 禁止缓存 */ 
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; /* 允许缓存 */
		store_cr0(cr0);
	}

	return i;
}

void memman_init(struct MEMMAN *man)
{
	man->frees = 0;    /* 可用信息数目 */
	man->maxfrees = 0; /* 用于观察可用状况：frees的最大值 */
	man->lostsize = 0; /* 释放失败的内存的大小总和 */
	man->losts = 0;    /* 释放失败次数 */
	return;
}

unsigned int memman_total(struct MEMMAN *man)
/* 报告空余内存大小的合计 */
{
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; i++) {
		t += man->free[i].size;
	}
	return t;
}

unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
/* 分配 */
{
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size >= size) {
			/* 找到了足够大的内存 */
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0) {
				/* 如果free[i]变成了0，就减掉一条可用信息 */
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1]; /* 代入结构体 */
				}
			}
			return a;
		}
	}
	return 0; /* 没有可用空间 */
}

int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
/* 释放 */
{
	int i, j;
	/* 为便于归纳内存，将free[]按照addr的顺序排列 */
	/* 所以，先决定应该放在哪里 */
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	/* free[i - 1].addr < addr < free[i].addr */
	if (i > 0) {
		/* 前面有可用内存 */
		if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
			/* 可以与前面的可用内存归纳到一起 */
			man->free[i - 1].size += size;
			if (i < man->frees) {
				/* 后面也有 */
				if (addr + size == man->free[i].addr) {
					/* 也可以与后面的可用内存归纳到一起 */
					man->free[i - 1].size += man->free[i].size;
					/* man->free[i]删除 */
					/* free[i]变成0后归纳到前面去 */
					man->frees--;
					for (; i < man->frees; i++) {
						man->free[i] = man->free[i + 1]; /* 结构体赋值 */
					}
				}
			}
			return 0; /* 成功完成 */
		}
	}
	/* 不能与前面的可用空间归纳到一起 */
	if (i < man->frees) {
		/* 后面还有 */
		if (addr + size == man->free[i].addr) {
			/* 可以与后面的内容归纳到一起 */
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0; /* 成功完成 */
		}
	}
	/* 既不能与前面归纳到一起，也不能与后面归纳到一起 */
	if (man->frees < MEMMAN_FREES) {
		/* free[i]之后的，向后移动，腾出一点可用空间 */
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; /* 更新最大值 */
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0; /* 成功完成 */
	}
	/* 不能往后移动 */
	man->losts++;
	man->lostsize += size;
	return -1; /* 失败 */
}

unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size)
{
	unsigned int a;
	size = (size + 0xfff) & 0xfffff000;
	a = memman_alloc(man, size);
	return a;
}

int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size)
{
	int i;
	size = (size + 0xfff) & 0xfffff000;
	i = memman_free(man, addr, size);
	return i;
}
