#include "../haribote/bootpack.h"
#include "pinyin_ime.h"
#include <string.h> // For strncmp, strlen

// Simplified Pinyin Dictionary
struct PinyinMapping {
    char pinyin[MAX_PINYIN_LEN];
    unsigned short gb2312_char;
};

// Example dictionary (Needs to be expanded significantly)
// GB2312 codes: 你 C4E3, 好 BAC3, 我 CED2, 是 CADC, 中 D6D0, 国 B9FA, 啊 B0A1
struct PinyinMapping pinyin_dict[] = {
    {"ni", 0xC4E3},
    {"hao", 0xBAC3},
    {"wo", 0xCED2},
    {"shi", 0xCADC},
    {"zhong", 0xD6D0},
    {"guo", 0xB9FA},
    {"a", 0xB0A1}
    // Add more entries here
};
const int pinyin_dict_size = sizeof(pinyin_dict) / sizeof(struct PinyinMapping); // Changed to const int

// Searches for candidate characters based on the current pinyin buffer
void search_candidates(struct TASK *task) {
    int i; // Moved declaration to the beginning of the block
    task->im_candidate_count = 0;
    task->im_selected_candidate = -1;
    if (task->im_pinyin_len == 0) {
        return;
    }

    int count = 0;
    for (i = 0; i < pinyin_dict_size && count < MAX_CANDIDATES_PER_PAGE; i++) { // Use declared i
        // Using exact match for simplicity for now.
        // A more advanced IME would use prefix matching, fuzzy matching, frequency, etc.
        if (task->im_pinyin_len == strlen(pinyin_dict[i].pinyin) &&
            strncmp(task->im_pinyin_buffer, pinyin_dict[i].pinyin, task->im_pinyin_len) == 0) {
            // Assuming gb2312_char is a 2-byte character
            task->im_candidate_chars[count][0] = (pinyin_dict[i].gb2312_char >> 8) & 0xFF; // High byte
            task->im_candidate_chars[count][1] = pinyin_dict[i].gb2312_char & 0xFF;        // Low byte
            task->im_candidate_chars[count][2] = '\0';                                   // Null terminator
            count++;
        }
    }
    task->im_candidate_count = count;
}

// Initializes IME-specific fields in the TASK structure
void pinyin_ime_init_task_fields(struct TASK *task) {
    task->im_input_mode = 0; // 0: direct ASCII, 1: Pinyin
    task->im_pinyin_len = 0;
    task->im_pinyin_buffer[0] = '\0';
    task->im_candidate_count = 0;
    task->im_selected_candidate = -1; // -1 means no candidate is actively selected by up/down keys (not implemented yet)
}

// Toggles between Pinyin input mode and direct ASCII mode
void pinyin_ime_toggle_mode(struct TASK *task, struct CONSOLE *cons) {
    if (task->langmode == 3) { // Only for GB2312 mode
        task->im_input_mode = 1 - task->im_input_mode;
        
        // Reset Pinyin buffer and candidates when mode changes
        task->im_pinyin_len = 0;
        task->im_pinyin_buffer[0] = '\0';
        task->im_candidate_count = 0;
        task->im_selected_candidate = -1;

        // Clear the IME UI display area
        if (cons && cons->sht) {
            int ime_ui_y = cons->cur_y + 16; 
            if (ime_ui_y + 15 >= cons->sht->bysize) { // If it goes off sheet, try last line
                ime_ui_y = cons->sht->bysize - 16;
                if (ime_ui_y < 0) ime_ui_y = 0; // Boundary check
            }
            unsigned char bg_col = COL8_000000; // Default background
            boxfill8(cons->sht->buf, cons->sht->bxsize, bg_col, 8, ime_ui_y, 8 + 240 - 1, ime_ui_y + 15);
            // sheet_refresh(cons->sht, 8, ime_ui_y, 8 + 240, ime_ui_y + 16); // pinyin_ime_display_ui will refresh this area
        }
        
        // Display the updated UI to reflect the new mode
        pinyin_ime_display_ui(task, cons); // <-- 添加这一行以刷新IME状态显示
    }
}

// Handles keyboard input when in Pinyin IME mode.
// Returns:
// >0: GB2312 character code committed.
//  0: Key consumed by IME for state change (e.g., pinyin buffer update, candidate search).
// -1: Key not consumed by IME (e.g., Enter on empty buffer, pass to console).
int pinyin_ime_handle_input(struct TASK *task, int keycode_char, struct CONSOLE *cons) {
    if (task->langmode != 3 || task->im_input_mode != 1) {
        return -1; // Not in Chinese Pinyin mode, IME does not handle.
    }

    if (keycode_char >= 'a' && keycode_char <= 'z') {
        if (task->im_pinyin_len < MAX_PINYIN_LEN -1) { // Ensure space for null terminator
            task->im_pinyin_buffer[task->im_pinyin_len++] = keycode_char;
            task->im_pinyin_buffer[task->im_pinyin_len] = '\0'; // Ensure null termination
            search_candidates(task);
            pinyin_ime_display_ui(task, cons); // Update UI
            return 0; // Consumed for pinyin buffer update
        }
    } else if (keycode_char >= '1' && keycode_char <= '9') {
        int cand_idx = keycode_char - '1'; // '1' selects 0th candidate
        if (cand_idx < task->im_candidate_count) {
            unsigned short selected_char = ((unsigned char)task->im_candidate_chars[cand_idx][0] << 8) | (unsigned char)task->im_candidate_chars[cand_idx][1];
            task->im_pinyin_len = 0;
            task->im_pinyin_buffer[0] = '\0';
            search_candidates(task); // Clears candidates
            pinyin_ime_display_ui(task, cons); // Update UI
            return selected_char;    // Return committed char
        }
    } else if (keycode_char == 8) { // Backspace
        if (task->im_pinyin_len > 0) {
            task->im_pinyin_len--;
            task->im_pinyin_buffer[task->im_pinyin_len] = '\0';
            search_candidates(task);
            pinyin_ime_display_ui(task, cons); // Update UI
            return 0; // Consumed
        } else {
            return -1; // Pinyin buffer empty, let console handle backspace.
        }
    } else if (keycode_char == 10) { // Enter
        if (task->im_pinyin_len > 0) { // If pinyin buffer is not empty
            if (task->im_candidate_count > 0) { // If there are candidates, commit the first one
                unsigned short selected_char = ((unsigned char)task->im_candidate_chars[0][0] << 8) | (unsigned char)task->im_candidate_chars[0][1];
                task->im_pinyin_len = 0;
                task->im_pinyin_buffer[0] = '\0';
                search_candidates(task); // Clears candidates and resets count
                pinyin_ime_display_ui(task, cons); // Update UI
                return selected_char;    // Return committed char
            } else { // No candidates, but pinyin buffer is not empty
                // Clear pinyin buffer
                task->im_pinyin_len = 0;
                task->im_pinyin_buffer[0] = '\0';
                search_candidates(task); // Clears candidates (already 0)
                pinyin_ime_display_ui(task, cons); // Update UI
                return 0; // Consumed, pinyin buffer cleared
            }
        } else { // Pinyin buffer empty
            return -1; // Let console handle Enter (e.g., run command)
        }
    } else if (keycode_char == ' ') { // Space key
        if (task->im_pinyin_len > 0) { // If pinyin buffer is not empty
            if (task->im_candidate_count > 0) { // If there are candidates, commit the first one
                unsigned short selected_char = ((unsigned char)task->im_candidate_chars[0][0] << 8) | (unsigned char)task->im_candidate_chars[0][1];
                task->im_pinyin_len = 0;
                task->im_pinyin_buffer[0] = '\0';
                search_candidates(task); // Clears candidates and resets count
                pinyin_ime_display_ui(task, cons); // Update UI
                return selected_char;    // Return committed char
            } else { // No candidates, but pinyin buffer is not empty
                // Clear pinyin buffer
                task->im_pinyin_len = 0;
                task->im_pinyin_buffer[0] = '\0';
                search_candidates(task); // Clears candidates (already 0)
                pinyin_ime_display_ui(task, cons); // Update UI
                return 0; // Consumed, pinyin buffer cleared
            }
        } else { // Pinyin buffer empty
            return -1; // Let console handle Space (e.g., print a space character)
        }
    }
    // If keycode_char was not handled by any of the above conditions in Pinyin mode,
    // but it's a printable char or other special key the IME might want to react to in the future.
    // For now, if it's not explicitly handled, we can choose to consume it or pass it.
    // Let's assume unhandled keys in pinyin mode (that are not a-z, 0-9, backspace, enter, space) are consumed and do nothing.
    // Or, more safely, pass them through if they are not typical IME control keys.
    // Given the current simple IME, if it's not one of the handled keys, it's better to return -1
    // to avoid swallowing unexpected characters.
    // However, if a key like Shift, Ctrl, Alt is pressed, it might come as a special keycode_char.
    // The current design assumes keycode_char is already a processed ASCII or simple key.
    return -1; // Key not handled by IME in Pinyin mode.
}

// Displays the Pinyin IME UI (pinyin buffer and candidates)
void pinyin_ime_display_ui(struct TASK *task, struct CONSOLE *cons) {
    int ime_ui_y; // Moved declaration
    int current_x; // Moved declaration
    char num_str[3]; // Moved declaration
    int i; // Moved declaration

    if (!cons || !cons->sht) {
        return;
    }

    // Determine Y position for IME UI. Try one line below cursor, fallback to last sheet line.
    ime_ui_y = cons->cur_y + 16;
    if (ime_ui_y + 15 >= cons->sht->bysize) {
        ime_ui_y = cons->sht->bysize - 16;
        if (ime_ui_y < 0) ime_ui_y = 0; // Ensure non-negative
    }
    
    unsigned char fg_col = COL8_FFFFFF;
    unsigned char bg_col = COL8_000000; // Console background color

    // Clear the IME UI line
    boxfill8(cons->sht->buf, cons->sht->bxsize, bg_col, 8, ime_ui_y, 8 + 240 - 1, ime_ui_y + 15);

    if (task->langmode == 3 && task->im_input_mode == 1) { // Only display if in Pinyin IME mode
        current_x = 8;
        // char num_str[3]; // For "1.", "2." etc. // Already declared

        // Display "Py: "
        putfonts8_asc_sht(cons->sht, current_x, ime_ui_y, fg_col, bg_col, "Py:", 3);
        current_x += 3 * 8;

        // Display Pinyin Buffer
        if (task->im_pinyin_len > 0) {
            putfonts8_asc_sht(cons->sht, current_x, ime_ui_y, fg_col, bg_col, task->im_pinyin_buffer, task->im_pinyin_len);
        }
        current_x += task->im_pinyin_len * 8 + 8; // Add some space after pinyin

        // Display Candidates
        if (task->im_candidate_count > 0 && current_x < 8 + 240 - (6*8)) { // Check space for "Cand: "
            putfonts8_asc_sht(cons->sht, current_x, ime_ui_y, fg_col, bg_col, "Cand:", 5);
            current_x += 5 * 8;

            for (i = 0; i < task->im_candidate_count; i++) { // Use declared i
                if (current_x + 16 + 8 + 16 > 8 + 240) break; // Check space for "1.字 "

                // Display number (e.g., "1.")
                num_str[0] = (i + 1) + '0';
                num_str[1] = '.';
                num_str[2] = 0;
                putfonts8_asc_sht(cons->sht, current_x, ime_ui_y, fg_col, bg_col, num_str, 2);
                current_x += 2 * 8;

                // Display candidate character (GB2312)
                // Correctly form the unsigned short from two chars to pass to putfont16_gb2312_sht if it expects bytes differently,
                // but putfont16_gb2312_sht takes b1 and b2 separately.
                // So, task->im_candidate_chars[i][0] and task->im_candidate_chars[i][1] are already the bytes.
                putfont16_gb2312_sht(cons->sht, current_x, ime_ui_y, fg_col, bg_col, 
                                     (unsigned char)task->im_candidate_chars[i][0], 
                                     (unsigned char)task->im_candidate_chars[i][1], 
                                     (char *)hzk16_font);
                current_x += 16; // GB char width

                putfonts8_asc_sht(cons->sht, current_x, ime_ui_y, fg_col, bg_col, " ", 1); // Separator
                current_x += 8;
            }
        }
    }
    // If not in pinyin mode, the line is already cleared by boxfill8 above.
    sheet_refresh(cons->sht, 8, ime_ui_y, 8 + 240, ime_ui_y + 16);
}

// Definition for putfont16_gb2312_sht
// Draws a 16x16 GB2312 character on a sheet using specified foreground and background colors.
// b1_in, b2_in are the two bytes of the GB2312 character.
// font_data_base_ptr should point to the HZK16 font data (e.g., hzk16_font). // Modified comment
void putfont16_gb2312_sht(struct SHEET *sht, int x, int y, int fg_col, int bg_col, unsigned char b1_in, unsigned char b2_in, char *font_data_base_ptr) {
    if (sht == 0 || font_data_base_ptr == 0) {
        return;
    }

    unsigned char *font_char_data;
    int i, j;

    // Check for valid GB2312 range (simplified check)
    // A1A1 - F7FE
    if (b1_in < 0xA1 || b1_in > 0xF7 || b2_in < 0xA1 || b2_in > 0xFE) {
        // Invalid GB2312 code, draw a placeholder (e.g., a 16x16 box)
        for (i = 0; i < 16; i++) {
            for (j = 0; j < 16; j++) {
                if (x + j >= 0 && x + j < sht->bxsize && y + i >= 0 && y + i < sht->bysize) {
                    sht->buf[(y + i) * sht->bxsize + (x + j)] = bg_col; // Fill with background
                }
            }
        }
        // Optionally draw a border for the placeholder
        if (x >= 0 && y >=0 && x + 15 < sht->bxsize && y + 15 < sht->bysize){
            for(i=0; i<16; i++){ // Top and bottom border
                 sht->buf[y * sht->bxsize + (x + i)] = fg_col;
                 sht->buf[(y+15) * sht->bxsize + (x+i)] = fg_col;
            }
            for(i=1; i<15; i++){ // Left and right border (avoiding corners)
                 sht->buf[(y+i) * sht->bxsize + x] = fg_col;
                 sht->buf[(y+i) * sht->bxsize + (x+15)] = fg_col;
            }
        }
        return;
    }

    unsigned int qm = b1_in - 0xA0;
    unsigned int wm = b2_in - 0xA0;
    unsigned int offset = ((qm - 1) * 94 + (wm - 1)) * 32; // Each char 32 bytes
    font_char_data = (unsigned char *)font_data_base_ptr + offset;

    for (i = 0; i < 16; i++) { // 16 rows
        if (y + i < 0 || y + i >= sht->bysize) continue; // Row out of sheet bounds

        unsigned char byte1 = font_char_data[i * 2];     // First 8 pixels
        unsigned char byte2 = font_char_data[i * 2 + 1]; // Next 8 pixels

        for (j = 0; j < 8; j++) { // Draw first 8 pixels
            if (x + j < 0 || x + j >= sht->bxsize) continue;
            if ((byte1 << j) & 0x80) {
                sht->buf[(y + i) * sht->bxsize + (x + j)] = fg_col;
            } else {
                sht->buf[(y + i) * sht->bxsize + (x + j)] = bg_col;
            }
        }
        for (j = 0; j < 8; j++) { // Draw next 8 pixels
            if (x + 8 + j < 0 || x + 8 + j >= sht->bxsize) continue;
            if ((byte2 << j) & 0x80) {
                sht->buf[(y + i) * sht->bxsize + (x + 8 + j)] = fg_col;
            } else {
                sht->buf[(y + i) * sht->bxsize + (x + 8 + j)] = bg_col;
            }
        }
    }
}

