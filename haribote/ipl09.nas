; haribote-ipl
; TAB=4

CYLS	EQU		55				; ǂ܂œǂݍނ

		ORG		0x7c00			; ���̃v���O�������ǂ��ɓǂݍ��܂��̂�

; �ȉ��͕W���I��FAT12�t�H�[�}�b�g�t���b�s�[�f�B�X�N�̂��߂̋L�q

		JMP		entry
		DB		0x90
		DB		"HARIBOTE"		; �u�[�g�Z�N�^�̖��O�����R�ɏ����Ă悢�i8�o�C�g�j
		DW		512				; 1�Z�N�^�̑傫���i512�ɂ��Ȃ���΂����Ȃ��j
		DB		1				; �N���X�^�̑傫���i1�Z�N�^�ɂ��Ȃ���΂����Ȃ��j
		DW		1				; FAT���ǂ�����n�܂邩�i���ʂ�1�Z�N�^�ڂ���ɂ���j
		DB		2				; FAT�̌��i2�ɂ��Ȃ���΂����Ȃ��j
		DW		224				; ���[�g�f�B���N�g���̈�̑傫���i���ʂ�224�G���g���ɂ���j
		DW		2880			; ���̃h���C�u�̑傫���i2880�Z�N�^�ɂ��Ȃ���΂����Ȃ��j
		DB		0xf0			; ���f�B�A�̃^�C�v�i0xf0�ɂ��Ȃ���΂����Ȃ��j
		DW		9				; FAT�̈�̒����i9�Z�N�^�ɂ��Ȃ���΂����Ȃ��j
		DW		18				; 1�g���b�N�ɂ����̃Z�N�^�����邩�i18�ɂ��Ȃ���΂����Ȃ��j
		DW		2				; �w�b�h�̐��i2�ɂ��Ȃ���΂����Ȃ��j
		DD		0				; �p�[�e�B�V�������g���ĂȂ��̂ł����͕K��0
		DD		2880			; ���̃h���C�u�傫����������x����
		DB		0,0,0x29		; �悭�킩��Ȃ����ǂ��̒l�ɂ��Ă����Ƃ����炵��
		DD		0xffffffff		; ���Ԃ�{�����[���V���A���ԍ�
		DB		"HARIBOTEOS "	; �f�B�X�N�̖��O�i11�o�C�g�j
		DB		"FAT12   "		; �t�H�[�}�b�g�̖��O�i8�o�C�g�j
		RESB	18				; �Ƃ肠����18�o�C�g�����Ă���

; �v���O�����{��

entry:
		MOV		AX,0			; ���W�X�^������
		MOV		SS,AX
		MOV		SP,0x7c00
		MOV		DS,AX

; �f�B�X�N��ǂ�

		MOV		AX,0x0820
		MOV		ES,AX
		MOV		CH,0			; �V�����_0
		MOV		DH,0			; �w�b�h0
		MOV		CL,2			; �Z�N�^2
		MOV		BX,18*2*CYLS-1	; �ǂݍ��݂������v�Z�N�^��
		CALL	readfast		; �����ǂݍ���

; �ǂݏI������̂�haribote.sys�����s���I

		MOV		BYTE [0x0ff0],CYLS	; IPL���ǂ��܂œǂ񂾂̂�������
		JMP		0xc200

error:
		MOV		AX,0
		MOV		ES,AX
		MOV		SI,msg
putloop:
		MOV		AL,[SI]
		ADD		SI,1			; SI��1�𑫂�
		CMP		AL,0
		JE		fin
		MOV		AH,0x0e			; �ꕶ���\���t�@���N�V����
		MOV		BX,15			; �J���[�R�[�h
		INT		0x10			; �r�f�IBIOS�Ăяo��
		JMP		putloop
fin:
		HLT						; ��������܂�CPU���~������
		JMP		fin				; �������[�v
msg:
		DB		0x0a, 0x0a		; ���s��2��
		DB		"load error"
		DB		0x0a			; ���s
		DB		0

readfast:	; AL���g���Ăł��邾���܂Ƃ߂ēǂݏo��
;	ES:�ǂݍ��ݔԒn, CH:�V�����_, DH:�w�b�h, CL:�Z�N�^, BX:�ǂݍ��݃Z�N�^��

		MOV		AX,ES			; < ES����AL�̍ő�l���v�Z >
		SHL		AX,3			; AX��32�Ŋ����āA���̌��ʂ�AH�ɓ��ꂽ���ƂɂȂ� �iSHL�͍��V�t�g���߁j
		AND		AH,0x7f			; AH��AH��128�Ŋ������]��i512*128=64K�j
		MOV		AL,128			; AL = 128 - AH; ��ԋ߂�64KB���E�܂ōő剽�Z�N�^���邩
		SUB		AL,AH

		MOV		AH,BL			; < BX����AL�̍ő�l��AH�Ɍv�Z >
		CMP		BH,0			; if (BH != 0) { AH = 18; }
		JE		.skip1
		MOV		AH,18
.skip1:
		CMP		AL,AH			; if (AL > AH) { AL = AH; }
		JBE		.skip2
		MOV		AL,AH
.skip2:

		MOV		AH,19			; < CL����AL�̍ő�l��AH�Ɍv�Z >
		SUB		AH,CL			; AH = 19 - CL;
		CMP		AL,AH			; if (AL > AH) { AL = AH; }
		JBE		.skip3
		MOV		AL,AH
.skip3:

		PUSH	BX
		MOV		SI,0			; ���s�񐔂𐔂��郌�W�X�^
retry:
		MOV		AH,0x02			; AH=0x02 : �f�B�X�N�ǂݍ���
		MOV		BX,0
		MOV		DL,0x00			; A�h���C�u
		PUSH	ES
		PUSH	DX
		PUSH	CX
		PUSH	AX
		INT		0x13			; �f�B�X�NBIOS�Ăяo��
		JNC		next			; �G���[�������Ȃ����next��
		ADD		SI,1			; SI��1�𑫂�
		CMP		SI,5			; SI��5���r
		JAE		error			; SI >= 5 ��������error��
		MOV		AH,0x00
		MOV		DL,0x00			; A�h���C�u
		INT		0x13			; �h���C�u�̃��Z�b�g
		POP		AX
		POP		CX
		POP		DX
		POP		ES
		JMP		retry
next:
		POP		AX
		POP		CX
		POP		DX
		POP		BX				; ES�̓��e��BX�Ŏ󂯎��
		SHR		BX,5			; BX��16�o�C�g�P�ʂ���512�o�C�g�P�ʂ�
		MOV		AH,0
		ADD		BX,AX			; BX += AL;
		SHL		BX,5			; BX��512�o�C�g�P�ʂ���16�o�C�g�P�ʂ�
		MOV		ES,BX			; ����� ES += AL * 0x20; �ɂȂ�
		POP		BX
		SUB		BX,AX
		JZ		.ret
		ADD		CL,AL			; CL��AL�𑫂�
		CMP		CL,18			; CL��18���r
		JBE		readfast		; CL <= 18 ��������readfast��
		MOV		CL,1
		ADD		DH,1
		CMP		DH,2
		JB		readfast		; DH < 2 ��������readfast��
		MOV		DH,0
		ADD		CH,1
		JMP		readfast
.ret:
		RET

		RESB	0x7dfe-$		; 0x7dfe�܂ł�0x00�Ŗ��߂閽��

		DB		0x55, 0xaa
