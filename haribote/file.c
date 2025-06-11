/* 文件相关函数 */

#include "bootpack.h"
#include <string.h>

///////
// file.c 新增和修改内容
struct MEM_DISK* get_memory_disk() {
    return &mem_disk;
}

// 从内存磁盘读取FAT表 - 由于已经在内存中了，所以可以不用
void file_readfat_from_memory(int* fat) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return;
    }
    memcpy(fat, disk->fat, FAT12_FAT_SIZE);
}

// 从内存磁盘读取文件
void file_loadfile_from_memory(int clustno, int size, char* buf, int* fat) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return;
    }

    // 计算数据起始簇号
    int data_start = FAT12_DATA_START / 512;
    int i;
    // 逐簇读取文件内容
    for (;;) {
        // 每次循环中，检查剩余文件大小 size 是否小于等于 512 字节
        // 如果是，则将剩余的 size 字节数据从当前簇复制到缓冲区 buf 中，并跳出循环
        if (size <= 512) {
            for (i = 0; i < size; i++) {
                buf[i] = disk->data[(clustno + data_start) * 512 + i];
            }
            break;
        }
        // 如果不是，则将当前簇的 512 字节数据复制到缓冲区 buf 中，然后更新剩余文件大小 size 和缓冲区指针 buf
        for (i = 0; i < 512; i++) {
            buf[i] = disk->data[(clustno + data_start) * 512 + i];
        }
        size -= 512;
        buf += 512;
        // 获取下一个簇号
        clustno = fat[clustno];
        // 检查下一个簇号是否有效
        if (clustno >= MAX_CLUSTERS || clustno == 0xfff) {
            break; // 文件结束或错误
        }
    }
}

// 写入文件到内存磁盘 - 覆盖写入
// 将指定大小的数据 buf 写入到内存磁盘中，从指定的簇号 clustno 开始
// 根据需要分配新的簇来存储数据，并在文件结束时正确标记 FAT 表
int file_writefile_to_memory(int clustno, int size, char* buf, int* fat) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return -1;
    }

    // 计算数据区域在磁盘中的起始簇号
    int data_start = FAT12_DATA_START / 512;
    int i, old_clust = clustno; // 保存文件的起始簇号，最后会作为函数的返回值
    int new_clust = clustno; // 用于跟踪当前正在写入数据的簇号
    
    // 循环写入数据 - 直到数据全部写入或没有可用簇为止
    for (;;) {
        // 如果剩余数据大小 size 小于等于 512 字节，则将剩余数据写入当前簇，并将该簇的 FAT 表项标记为文件结束
        if (size <= 512) {
            for (i = 0; i < size; i++) {
                disk->data[(new_clust + data_start) * 512 + i] = buf[i];
            }
            // 标记文件结束
            fat[new_clust] = 0xfff;
            break;
        }
        
        // 如果剩余数据大小大于 512 字节，则将 512 字节的数据写入当前簇
        for (i = 0; i < 512; i++) {
            disk->data[(new_clust + data_start) * 512 + i] = buf[i];
        }

        // 更新剩余数据大小 size 和缓冲区指针 buf
        size -= 512;
        buf += 512;
        
        // 分配新簇 - 得到下一簇的簇号
        int next_clust = alloc_cluster(fat);
        if (next_clust < 0) { // -1
            // 没有可用簇
            fat[new_clust] = 0xfff; // 当前簇为文件终结
            break;
        }
        
        // 更新 FAT 表 
        fat[new_clust] = next_clust;
        new_clust = next_clust;
    }
    
    return old_clust; // 返回起始簇号
}

// 分配新簇
int alloc_cluster(int* fat) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return -1;
    }
    
    int i;
    // 簇号 0 和 1：保留给 FAT 表本身使用，不用于数据存储
    // 簇号 2：通常用作根目录的起始簇（对于支持根目录在数据区的 FAT12 变体
    for (i = 3; i < MAX_CLUSTERS; i++) {
        if (fat[i] == 0) {
            fat[i] = 0xfff; // 标记为结束，后续调用时会更新FAT表 - 之前错写为0
            return i;
        }
    }
    return -1; // 没有可用簇
}

// 在当前工作目录下，在内存磁盘的对应位置中创建文件
struct FILEINFO* create_file_in_memory(struct FILEINFO* parent_dir, const char* name, int size) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return 0;
    }
    
    // 确定父目录位置
    struct FILEINFO* dir = parent_dir ? parent_dir : disk->root_dir;
    
    // 查找父目录下的空目录项（已二次定位）
    struct FILEINFO* finfo = find_free_file_info(dir,disk->fat);
    if (!finfo) {
        return 0; // 目录已满
    }
    
    // 解析文件名和扩展名
    char short_name[11] = {0};
    parse_short_name(name, short_name);
    
    // 复制文件名
    strncpy(finfo->name, short_name, 8);
    strncpy(finfo->ext, short_name + 8, 3);
    
    // 为文件的数据分配簇
    int clustno = alloc_cluster(disk->fat);
    if (clustno < 0) {
        return 0; // 无法分配簇
    }
    
    // 设置文件信息 - entry
    finfo->clustno = clustno;
    finfo->size = size;
    finfo->name[0] &= 0x7f; // 清除删除标志
    
    return finfo;
}


// 在当前工作目录下，在内存磁盘的对应位置中创建目录
struct FILEINFO* create_directory_in_memory(struct FILEINFO* parent_dir, const char* name) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return 0;
    }

    // 确定父目录位置
    struct FILEINFO* dir = parent_dir ? parent_dir : disk->root_dir;
    
    // 查找父目录中的空闲目录项 - 会跳过 . 与 .. 项 (已经通过二次定位)
    struct FILEINFO* finfo = find_free_file_info(dir,disk->fat);
    if (!finfo) {
        return 0; // 目录已满
    }

    // 解析并转换为8.3格式文件名
    char short_name[11] = {0};
    parse_short_name(name, short_name);

    // 在父目录下新增entry -目录名和扩展名
    strncpy(finfo->name, short_name, 8);
    strncpy(finfo->ext, short_name + 8, 3);

    // 分配簇用于存储 新创创建 目录内容（entry）
    int clustno = alloc_cluster(disk->fat);
    if (clustno < 0) {
        return 0; // 无法分配簇
    }

    // 设置目录信息 - entry
    finfo->clustno = clustno;
    finfo->size = 0; // 目录大小为0
    finfo->name[0] &= 0x7f; // 清除删除标志
    finfo->type |= 0x10;    // 标记为目录

    // 初始化 新目录中的初始条目 (添加 . 和 .. 条目)
    struct FILEINFO* new_dir = (struct FILEINFO*)&disk->data[(clustno + (FAT12_DATA_START / 512)) * 512];
    
    // . 条目
    strcpy(new_dir[0].name, finfo->name);
    strcpy(new_dir[0].ext, finfo->ext);
    new_dir[0].type = 0x10; // 目录
    new_dir[0].clustno = clustno;
    new_dir[0].size = 0;
    
    // .. 条目
    if (parent_dir) {
        strcpy(new_dir[1].name, parent_dir->name);
        strcpy(new_dir[1].ext, parent_dir->ext);
        new_dir[1].type = 0x10; // 目录
        new_dir[1].clustno = parent_dir->clustno;
        new_dir[1].size = 0;
    } else {
        // 根目录的父目录指向自身
        strcpy(new_dir[1].name, finfo->name);
        strcpy(new_dir[1].ext, finfo->ext);
        new_dir[1].type = 0x10; // 目录
        new_dir[1].clustno = clustno;
        new_dir[1].size = 0;
    }

    return finfo;
}

// 在当前工作目录下查找空闲entry项，注意传入的参数也是 entry 因此要二次定位
// 区分根目录和非根目录情况
struct FILEINFO* find_free_file_info(struct FILEINFO* root_dir, int* fat) {
    struct MEM_DISK* disk = get_memory_disk();
    if (!disk->initialized) {
        return 0;
    }

    struct FILEINFO* search_dir;
    int max_entries;
    // 跳过根目录第一个占位项
    int start_index = 1;

    if (root_dir == disk->root_dir) {
        // 根目录情况
        search_dir = root_dir;
        max_entries = 224;
    } else {
        // 非根目录情况
        int data_start = FAT12_DATA_START / 512;
        int clustno = root_dir->clustno;
        search_dir = (struct FILEINFO*)&disk->data[(clustno + data_start) * 512];
        max_entries = 16;  // 单簇目录先限制为16个条目
        start_index = 2;   // 跳过 . 和 .. 条目
    }

    int i;
    for (i = start_index; i < max_entries; i++) {
        if (search_dir[i].name[0] == 0) {
            return &search_dir[i];
        }
        if ((search_dir[i].name[0] & 0xe5) == 0xe5) {
            // 已删除的文件项，可重用
            return &search_dir[i];
        }
    }

    if (i >= max_entries) {
        // 超出单簇目录限制
        // 返回 0 表示未找到空闲项
        return 0;
    }

    return 0; // 没有空闲项
}

// 解析短文件名
void parse_short_name(const char* long_name, char* short_name) {
    // 简化实现，将长文件名转换为8.3格式
    memset(short_name, ' ', 11);
    int i, j = 0;
    
    // 复制文件名部分
    for (i = 0; i < 8 && long_name[i] && long_name[i] != '.'; i++) {
        if (long_name[i] >= 'a' && long_name[i] <= 'z') {
            short_name[j] = long_name[i] - 'a' + 'A';
        } else {
            short_name[j] = long_name[i];
        }
        j++;
    }
    
    // 复制扩展名部分
    if (long_name[i] == '.') {
        i++;
        j = 8;
		int k = 0;
        for (k = 0; k < 3 && long_name[i]; k++, i++) {
            if (long_name[i] >= 'a' && long_name[i] <= 'z') {
                short_name[j] = long_name[i] - 'a' + 'A';
            } else {
                short_name[j] = long_name[i];
            }
            j++;
        }
    }
}
//////


void file_readfat(int *fat, unsigned char *img)
/*将磁盘映像中的FAT解压缩 */
{
	int i, j = 0;
	for (i = 0; i < 2880; i += 2) {
		fat[i + 0] = (img[j + 0]      | img[j + 1] << 8) & 0xfff;
		fat[i + 1] = (img[j + 1] >> 4 | img[j + 2] << 4) & 0xfff;
		j += 3;
	}
	return;
}

void file_loadfile(int clustno, int size, char *buf, int *fat, char *img)
{
	int i;
	for (;;) {
		if (size <= 512) {
			for (i = 0; i < size; i++) {
				buf[i] = img[clustno * 512 + i];
			}
			break;
		}
		for (i = 0; i < 512; i++) {
			buf[i] = img[clustno * 512 + i];
		}
		size -= 512;
		buf += 512;
		clustno = fat[clustno];
	}
	return;
}

struct FILEINFO *file_search(char *name, struct FILEINFO *finfo, int max)
{
	int i, j;
	char s[12];
	for (j = 0; j < 11; j++) {
		s[j] = ' ';
	}
	j = 0;
	for (i = 0; name[i] != 0; i++) {
		if (j >= 11) { return 0; /*没有找到*/ }
		if (name[i] == '.' && j <= 8) {
			j = 8;
		} else {
			s[j] = name[i];
			if ('a' <= s[j] && s[j] <= 'z') {
				/*将小写字母转换为大写字母*/
				s[j] -= 0x20;
			} 
			j++;
		}
	}
	for (i = 0; i < max; ) {
		if (finfo->name[0] == 0x00) {
			break;
		}
		if ((finfo[i].type & 0x18) == 0) {
			for (j = 0; j < 11; j++) {
				if (finfo[i].name[j] != s[j]) {
					goto next;
				}
			}
			return finfo + i; /*找到文件*/
		}
next:
		i++;
	}
	return 0; /*没有找到*/
}

char *file_loadfile2(int clustno, int *psize, int *fat)
{
	int size = *psize, size2;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	char *buf, *buf2;
	buf = (char *) memman_alloc_4k(memman, size);
	file_loadfile(clustno, size, buf, fat, (char *) (ADR_DISKIMG + 0x003e00));
	if (size >= 17) {
		size2 = tek_getsize(buf);
		if (size2 > 0) {	/*使用tek格式压缩的文件*/
			buf2 = (char *) memman_alloc_4k(memman, size2);
			tek_decomp(buf, buf2, size2);
			memman_free_4k(memman, (int) buf, size);
			buf = buf2;
			*psize = size2;
		}
	}
	return buf;
}
