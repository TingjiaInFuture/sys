/* 定时器相关 */

#include "bootpack.h"

#define PIT_CTRL	0x0043
#define PIT_CNT0	0x0040

struct TIMERCTL timerctl;

#define TIMER_FLAGS_ALLOC		1	/* 已分配 */
#define TIMER_FLAGS_USING		2	/* 定时器运行中 */

// Helper function to swap two timers in the heap
void swap_timers(int i, int j) {
	struct TIMER *tmp = timerctl.heap[i];
	timerctl.heap[i] = timerctl.heap[j];
	timerctl.heap[j] = tmp;
	timerctl.heap[i]->heap_index = i;
	timerctl.heap[j]->heap_index = j;
}

// Min-heap sift up operation
void heap_siftup(int index) {
	while (index > 0) {
		int parent = (index - 1) / 2;
		if (timerctl.heap[parent]->timeout > timerctl.heap[index]->timeout) {
			swap_timers(parent, index);
			index = parent;
		} else {
			break;
		}
	}
}

// Min-heap sift down operation
void heap_siftdown(int index) {
	int size = timerctl.heap_size;
	while (index * 2 + 1 < size) {
		int left_child = index * 2 + 1;
		int right_child = index * 2 + 2;
		int smallest = index;

		if (timerctl.heap[left_child]->timeout < timerctl.heap[smallest]->timeout) {
			smallest = left_child;
		}
		if (right_child < size && timerctl.heap[right_child]->timeout < timerctl.heap[smallest]->timeout) {
			smallest = right_child;
		}

		if (smallest != index) {
			swap_timers(index, smallest);
			index = smallest;
		} else {
			break;
		}
	}
}

// Insert a timer into the heap
void heap_insert(struct TIMER *timer) {
	if (timerctl.heap_size >= MAX_TIMER) {
		return; // Heap is full
	}
	int index = timerctl.heap_size++;
	timerctl.heap[index] = timer;
	timer->heap_index = index;
	heap_siftup(index);
}

// Remove a timer from the heap (general case)
void heap_remove(int index) {
    if (index < 0 || index >= timerctl.heap_size) {
        return; // Invalid index
    }
    int last_index = --timerctl.heap_size;
    if (index == last_index) {
        timerctl.heap[index]->heap_index = -1; // Mark as removed from heap
        return; // Removing the last element
    }

    // Swap with the last element
    swap_timers(index, last_index);
    timerctl.heap[last_index]->heap_index = -1; // Mark original timer as removed

    // Restore heap property
    // Check if the new element at 'index' needs to go up or down
    if (index > 0 && timerctl.heap[(index - 1) / 2]->timeout > timerctl.heap[index]->timeout) {
        heap_siftup(index);
    } else {
        heap_siftdown(index);
    }
}


void init_pit(void)
{
	int i;
	io_out8(PIT_CTRL, 0x34);
	io_out8(PIT_CNT0, 0x9c); /* 中断周期 11932 (0x2e9c)，约 100Hz (10ms) */
	io_out8(PIT_CNT0, 0x2e);
	timerctl.count = 0;
	timerctl.heap_size = 0; // Initialize heap size
	for (i = 0; i < MAX_TIMER; i++) {
		timerctl.timers0[i].flags = 0; /* 未使用 */
		timerctl.timers0[i].heap_index = -1; // Not in heap initially
	}
	// No need for sentinel timer anymore
	return;
}

struct TIMER *timer_alloc(void)
{
	int i;
	for (i = 0; i < MAX_TIMER; i++) {
		if (timerctl.timers0[i].flags == 0) {
			timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
			timerctl.timers0[i].flags2 = 0;
			timerctl.timers0[i].heap_index = -1; // Ensure it's marked as not in heap
			return &timerctl.timers0[i];
		}
	}
	return 0; /* 没找到 */
}

void timer_free(struct TIMER *timer)
{
    if (timer->flags == TIMER_FLAGS_USING) { // If it's currently active, cancel it first
        timer_cancel(timer);
    }
	timer->flags = 0; /* 未使用 */
	timer->heap_index = -1;
	return;
}

void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data)
{
	timer->fifo = fifo;
	timer->data = data;
	timer->interval = 0; // Default to non-periodic
	return;
}

// Modified timer_settime to include interval
void timer_settime(struct TIMER *timer, unsigned int timeout, unsigned int interval)
{
	int e;
	// If timeout is relative (e.g., 50 for 500ms), make it absolute
	// If timeout is already absolute, this addition might be incorrect depending on usage.
	// Assuming timeout is relative delay here.
	timer->timeout = timeout + timerctl.count;
	timer->interval = interval; // Store the interval
	timer->flags = TIMER_FLAGS_USING;
	e = io_load_eflags();
	io_cli();
    // If timer was already in heap, remove it first
    if (timer->heap_index != -1) {
        heap_remove(timer->heap_index);
    }
	heap_insert(timer); // Insert into the heap
	io_store_eflags(e);
	return;
}

void inthandler20(int *esp)
{
	struct TIMER *timer;
	char ts = 0; // Task switch flag
	io_out8(PIC0_OCW2, 0x60);	/* 把IRQ-00信号接收完了的信息通知给PIC */
	timerctl.count++;

	// Process all expired timers
	while (timerctl.heap_size > 0 && timerctl.heap[0]->timeout <= timerctl.count) {
		timer = timerctl.heap[0]; // Get the timer with the smallest timeout
		timer->flags = TIMER_FLAGS_ALLOC; // Mark as allocated (not actively timing)

		// Send data if not the task timer
		if (timer != task_timer) {
			fifo32_put(timer->fifo, timer->data);
		} else {
			ts = 1; /* task_timer超时了 */
		}

        // Remove the expired timer from the heap root
        heap_remove(0); // This also adjusts heap_size

		// If it's a periodic timer, re-insert it with the next timeout
		if (timer->interval > 0) {
			timer->timeout = timerctl.count + timer->interval;
			timer->flags = TIMER_FLAGS_USING; // Mark as active again
            heap_insert(timer); // Re-insert into heap
		} else {
            // One-shot timer is done, heap_remove already marked index as -1
        }
	}

	if (ts != 0) {
		task_switch();
	}

	return;
}

int timer_cancel(struct TIMER *timer)
{
	int e;
	e = io_load_eflags();
	io_cli();	/* 在设置过程中禁止改变定时器状态 */
	if (timer->flags == TIMER_FLAGS_USING && timer->heap_index != -1) {	/* 需要取消吗？ */
		heap_remove(timer->heap_index); // Remove from heap
		timer->flags = TIMER_FLAGS_ALLOC; // Mark as allocated, not free yet
		io_store_eflags(e);
		return 1;	/* 取消处理成功 */
	}
	io_store_eflags(e);
	return 0; /* 不需要取消处理 */
}

void timer_cancelall(struct FIFO32 *fifo)
{
	int e, i;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli();	/* 在设置过程中禁止改变定时器状态 */
	for (i = 0; i < MAX_TIMER; i++) {
		t = &timerctl.timers0[i];
		// Check if timer is allocated/using, belongs to the fifo, and potentially in the heap
		if ((t->flags == TIMER_FLAGS_ALLOC || t->flags == TIMER_FLAGS_USING) && t->flags2 == 0 && t->fifo == fifo) {
            if (t->flags == TIMER_FLAGS_USING && t->heap_index != -1) { // If active and in heap
			    timer_cancel(t); // Cancel removes from heap and sets flags to ALLOC
            }
			timer_free(t); // Now mark as completely free
		}
	}
	io_store_eflags(e);
	return;
}
