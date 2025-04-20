[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api018.nas"]

		GLOBAL	_api_settimer

[SECTION .text]

_api_settimer:		; void api_settimer(int timer, int time, int interval);
		PUSH	EBX
		PUSH	ECX         ; Preserve ECX
		MOV		EDX,18
		MOV		EBX,[ESP+12]	; timer
		MOV		EAX,[ESP+16]	; time
		MOV		ECX,[ESP+20]	; interval (New parameter)
		INT		0x40
		POP		ECX         ; Restore ECX
		POP		EBX
		RET
