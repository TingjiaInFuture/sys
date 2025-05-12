#include "../haribote/bootpack.h" // For struct TASK, struct CONSOLE
#include "pinyin_ime.h"
#include <string.h> // For strlen, strncmp, sprintf
#include <stdio.h>    // 新增：支持 sprintf

// Pinyin to GB2312 mapping
// This dictionary needs to be comprehensive for practical use.
// GB2312 codes: 你 C4E3, 好 BAC3, 我 CED2, 是 CADC, 中 D6D0, 国 B9FA, 啊 B0A1
// 爱 B0AE, 不 B2BB, 吃 C3D4, 的 B5C4, 字 D7D6, 他 DCB9, 她 DCDA, 它 DCDD
// 吗 C2F0, 呢 C4D8, 吧 B0C9, 了 C1CB, 很 BACE, 谢 D0BB, 再 D4D9, 见 BCAF
// 请 C7EB, 问 CEC4, 答 B4F0, 对 B6D4, 错 B4ED, 明 C3F7, 天 CCEC, 月 D4C2, 日 C8D5
struct PinyinMapping pinyin_dict[] = {
    {"ni", 0xC4E3}, {"hao", 0xBAC3}, {"wo", 0xCED2}, {"shi", 0xCADC},
    {"zhong", 0xD6D0}, {"guo", 0xB9FA}, {"a", 0xB0A1}, {"ai", 0xB0AE},
    {"bu", 0xB2BB}, {"chi", 0xC3D4}, {"de", 0xB5C4}, {"zi", 0xD7D6},
    {"ta", 0xDCB9},  // 他
    {"ta", 0xDCDA},  // 她
    {"ta", 0xDCDD},  // 它
    {"ma", 0xC2F0}, {"ne", 0xC4D8}, {"ba", 0xB0C9}, {"le", 0xC1CB},
    {"hen", 0xBACE}, {"xie", 0xD0BB}, {"zai", 0xD4D9}, {"jian", 0xBCAF},
    {"qing", 0xC7EB}, {"wen", 0xCEC4}, {"da", 0xB4F0}, {"dui", 0xB6D4},
    {"cuo", 0xB4ED}, {"ming", 0xC3F7}, {"tian", 0xCCEC}, {"yue", 0xD4C2}, 
    {"ri", 0xC8D5}
    // Add more entries here
};
const int pinyin_dict_size = sizeof(pinyin_dict) / sizeof(struct PinyinMapping);

// Searches the dictionary for Pinyin matches and populates candidate list
// Modifies task->im_candidate_chars and task->im_candidate_count
void search_candidates(struct TASK *task) {
    int count = 0;
    if (task->im_pinyin_len == 0) {
        task->im_candidate_count = 0;
        return;
    }
    for (int i = 0; i < pinyin_dict_size && count < MAX_CANDIDATES; i++) {
        // Match if the pinyin in buffer is a prefix of a dictionary entry
        if (task->im_pinyin_len <= strlen(pinyin_dict[i].pinyin) &&
            strncmp(task->im_pinyin_buffer, pinyin_dict[i].pinyin, task->im_pinyin_len) == 0) {
            task->im_candidate_chars[count][0] = (pinyin_dict[i].gb2312_char >> 8) & 0xFF; // High byte
            task->im_candidate_chars[count][1] = pinyin_dict[i].gb2312_char & 0xFF;        // Low byte
            count++; // Increment count only when a candidate is added
        }
    }
    task->im_candidate_count = count;
    task->im_selected_candidate = -1; // Reset selection when candidates change
}

// Initializes Pinyin IME specific fields in the TASK struct
void pinyin_ime_init_task_fields(struct TASK *task) {
    task->im_input_mode = 0; // 0: direct ASCII, 1: Pinyin
    task->im_pinyin_len = 0;
    task->im_pinyin_buffer[0] = '\0';
    task->im_candidate_count = 0;
    task->im_selected_candidate = -1; // -1 means no candidate is actively selected
}

// Toggles between Pinyin mode and ASCII mode
void pinyin_ime_toggle_mode(struct TASK *task, struct CONSOLE *cons) { // Added cons parameter
    task->im_input_mode = 1 - task->im_input_mode;
    // Clear Pinyin buffer and candidates when switching modes
    task->im_pinyin_len = 0;
    task->im_pinyin_buffer[0] = '\0';
    task->im_candidate_count = 0;
    task->im_selected_candidate = -1;
}

// Handles a character input when IME is active.
// Returns: GB2312 char code if committed, 0 if key consumed by IME, PINYIN_IME_KEY_NOT_CONSUMED if key not consumed
unsigned short pinyin_ime_handle_input(struct TASK *task, unsigned char key_code, struct CONSOLE *cons) {
    // If not in Chinese mode (langmode == 3) or Pinyin input mode is off, do not consume the key.
    if (task->langmode != 3 || task->im_input_mode != 1) {
        return PINYIN_IME_KEY_NOT_CONSUMED;
    }

    // Ctrl + Space: Toggle IME on/off - This is a common toggle, actual key might differ.
    // For now, we assume a dedicated key or OS mechanism handles toggling task->im_input_mode.

    if (key_code >= 'a' && key_code <= 'z') { // Alphabetical input for Pinyin
        if (task->im_pinyin_len < MAX_PINYIN_LEN - 1) { // Ensure space for null terminator
            task->im_pinyin_buffer[task->im_pinyin_len++] = key_code;
            task->im_pinyin_buffer[task->im_pinyin_len] = '\0';
            search_candidates(task);
            return 0; // Key consumed
        }
    } else if (key_code >= '1' && key_code <= '0' + MAX_CANDIDATES_PER_PAGE) { // Number keys for candidate selection
        int cand_idx = key_code - '1';
        if (cand_idx >= 0 && cand_idx < task->im_candidate_count) {
            unsigned short gb_char_high = task->im_candidate_chars[cand_idx][0];
            unsigned short gb_char_low = task->im_candidate_chars[cand_idx][1];
            unsigned short gb_char = (gb_char_high << 8) | gb_char_low;

            // Clear Pinyin buffer and candidates
            task->im_pinyin_len = 0;
            task->im_pinyin_buffer[0] = '\0';
            task->im_candidate_count = 0;
            task->im_selected_candidate = -1;
            return gb_char; // Commit selected character
        }
    } else if (key_code == 0x08) { // Backspace
        if (task->im_pinyin_len > 0) {
            task->im_pinyin_len--;
            task->im_pinyin_buffer[task->im_pinyin_len] = '\0';
            search_candidates(task);
            return 0; // Key consumed
        } else {
            // Pinyin buffer is empty, backspace should be handled by console
            return PINYIN_IME_KEY_NOT_CONSUMED;
        }
    } else if (key_code == ' ' || key_code == 0x0a ) { // Space or Enter to commit first candidate or typed pinyin as is (if no candidates)
        if (task->im_pinyin_len > 0) { // If pinyin buffer is not empty
            if (task->im_candidate_count > 0) {
                // Commit the first candidate
                unsigned short gb_char_high = task->im_candidate_chars[0][0];
                unsigned short gb_char_low = task->im_candidate_chars[0][1];
                unsigned short gb_char = (gb_char_high << 8) | gb_char_low;

                task->im_pinyin_len = 0;
                task->im_pinyin_buffer[0] = '\0';
                task->im_candidate_count = 0;
                task->im_selected_candidate = -1;
                return gb_char; // Commit first candidate
            } else {
                // No candidates, but pinyin buffer is not empty.
                // Option 1: Clear buffer and consume key (current behavior)
                task->im_pinyin_len = 0;
                task->im_pinyin_buffer[0] = '\0';
                task->im_candidate_count = 0; // Should be 0 already
                return 0; // Key consumed, nothing committed
                // Option 2: Commit pinyin as ASCII (not typical for Chinese IME)
                // Option 3: Do nothing, let console handle (might be confusing)
            }
        }
        // If pinyin buffer is empty, space/enter should be handled by the console.
        return PINYIN_IME_KEY_NOT_CONSUMED; 
    }
    // Other keys (ESC, arrows for candidate navigation - not implemented yet) could be handled here.
    // For now, other keys are not consumed by the IME if Pinyin mode is active.
    return PINYIN_IME_KEY_NOT_CONSUMED; 
}

// Displays the Pinyin IME UI (pinyin buffer and candidates)
// This function needs to be called from the console's refresh logic when appropriate.
void pinyin_ime_display_ui(struct TASK *task, struct CONSOLE *cons) {
    if (!cons || !cons->sht) return; // Safety check

    // Define a fixed UI area at the bottom of the console's text box
    // Assuming console text box starts at y=28 and has height 128.
    int ui_base_x = 8;
    int ui_base_y = 28 + 128 - 16; // Last line of the text box area
    int ui_width = cons->sht->bxsize - 16; // Use sheet width, leave some margin
    int ui_height = 16;            // Height for one line of text

    // Clear the IME UI area first (using background color of the text box, typically COL8_000000)
    boxfill8(cons->sht->buf, cons->sht->bxsize, COL8_000000, ui_base_x, ui_base_y, ui_base_x + ui_width - 1, ui_base_y + ui_height - 1);

    if (task->im_input_mode == 1 && task->langmode == 3) { // Only display if in Pinyin input mode and langmode is Chinese
        char pinyin_display_str[MAX_PINYIN_LEN + 16]; // Buffer for "Pinyin:[...]"
        char candidate_num_str[4]; // Buffer for "1." etc.

        int current_x = ui_base_x;

        // Display Pinyin buffer
        sprintf(pinyin_display_str, "Pinyin:[%s]", task->im_pinyin_buffer);
        putfonts8_asc_sht(cons->sht, current_x, ui_base_y, COL8_FFFFFF, COL8_000000, pinyin_display_str, strlen(pinyin_display_str));
        current_x += strlen(pinyin_display_str) * 8; // Advance X based on actual length

        // Display candidates (add some spacing)
        current_x += 16; // More space after Pinyin buffer

        for (int i = 0; i < task->im_candidate_count && i < MAX_CANDIDATES_PER_PAGE; i++) {
            if (current_x + 8 + 16 > ui_base_x + ui_width) break; // Stop if not enough space

            // Display candidate number (e.g., "1.")
            sprintf(candidate_num_str, "%d.", i + 1);
            putfonts8_asc_sht(cons->sht, current_x, ui_base_y, COL8_FFFFFF, COL8_000000, candidate_num_str, strlen(candidate_num_str));
            current_x += strlen(candidate_num_str) * 8;

            // Display the GB2312 character
            // Ensure putfont16_gb2312_sht is correctly implemented and linked.
            // It expects the two bytes of the GB2312 character separately.
            putfont16_gb2312_sht(cons->sht, current_x, ui_base_y, COL8_FFFFFF, COL8_000000, task->im_candidate_chars[i][0], task->im_candidate_chars[i][1]);
            current_x += 16; // GB2312 characters are 16 pixels wide

            current_x += 8; // Some spacing between candidates
        }
    }
    // After drawing (or clearing), refresh the modified UI area on the sheet
    sheet_refresh(cons->sht, ui_base_x, ui_base_y, ui_base_x + ui_width, ui_base_y + ui_height);
}

