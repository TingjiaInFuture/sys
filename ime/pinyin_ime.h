#ifndef PINYIN_IME_H
#define PINYIN_IME_H

#define MAX_PINYIN_LEN 10         // Maximum length of Pinyin input
#define MAX_CANDIDATES_PER_PAGE 5 // Maximum candidates to show

#include "../haribote/bootpack.h" // For TASK struct and CONSOLE struct

// Public functions for the Pinyin IME
void pinyin_ime_init_task_fields(struct TASK *task);
void pinyin_ime_toggle_mode(struct TASK *task, struct CONSOLE *cons);

// Handles a character input when IME is active.
// Returns:
//  >0: GB2312 character code committed to the console.
//   0: Character consumed by IME (e.g., added to Pinyin buffer, candidates updated, no output to console).
//  -1: Character not consumed by IME (should be processed normally by console).
int pinyin_ime_handle_input(struct TASK *task, int keycode_char, struct CONSOLE *cons);

// Displays the current Pinyin buffer and candidate list on the console.
void pinyin_ime_display_ui(struct TASK *task, struct CONSOLE *cons);

// Function to draw a GB2312 character on a sheet (defined in pinyin_ime.c)
void putfont16_gb2312_sht(struct SHEET *sht, int x, int y, int c, int b, unsigned char b1, unsigned char b2, char *font_data_ptr);

#endif // PINYIN_IME_H
