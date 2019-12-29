#include "includes.h"

#include "data/DataAVR.h"

#include "data/test1.h"
#include "data/test2.h"
#include "data/test3.h"
#include "data/sml_short.h"
#include "data/pallet_town_short.h"
#include "data/smb_short.h"

// 상수 및 매크로 정의
#define TASK_STK_SIZE	OS_TASK_DEF_STK_SIZE
#define N_TASKS			3
#define PIP_PRIO		0
#define N_MAX_MUSICS	8	// 최대 수록 가능 음악 수
#define N_MUSICS		6	// 음악의 개수

const INT8U fnd_digit[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x27, 0x7f, 0x6f };
const INT8U fnd_alpha[] = { 0x73, 0x38, 0x77, 0x66, 0x3e, 0x6d, 0x78, 0x3f }; // P, L, A, Y, U, S, t, O
const INT8U fnd_sel[4] = { 0x01,0x02,0x04,0x08 };

enum {
	STAT_STOP = 0,
	STAT_PLAY,
	STAT_PAUSE
};

// 전역변수
OS_STK taskStk[N_TASKS][TASK_STK_SIZE];

OS_EVENT *msgRand;
OS_FLAG_GRP *flagState; // os_cfg.h 에서 플래그 크기를 8비트로 정의하고 있음

// INT8U noteSizes[N_MUSICS] = { 12, 29, 28, 216 };
// const NoteDataAVR * const musicList[N_MUSICS] = { test1, test2, test3, sml };

// INT8U noteSizes[N_MUSICS] = { 12, 29, 216, 152 };
// const NoteDataAVR * const musicList[N_MUSICS] = { test1, test2, sml, smb_short };

INT8U noteSizes[N_MUSICS] = { 12, 29, 28, 152, 76, 113 };
const NoteDataAVR * const musicList[N_MUSICS] = { test1, test2, test3, smb_short, pallet_town_short, sml_short };

volatile INT8U curOvfCnt = 0;
volatile BOOLEAN compLoop = FALSE;

volatile const NoteDataAVR *curMusic = NULL;
volatile INT8U curMusicIdx = 0;
volatile INT16U curNoteIdx = 0;
volatile INT8U status = STAT_STOP;
volatile BOOLEAN buzStatus = FALSE;	// 부저의 on/off

// 함수 선언부
void initRegister(void);

void randTask(void *pdata);
void musicTask(void *pdata);
void fndTask(void *pdata);

ISR(INT4_vect)
{
	if (status != STAT_STOP) {
		curNoteIdx = 0;
	}
}

ISR(INT5_vect)
{
	if (status == STAT_PLAY) {
		status = STAT_PAUSE;
	}
	else if (status == STAT_PAUSE) {
		status = STAT_PLAY;
	}
}

ISR(TIMER2_OVF_vect)
{
	// loopCnt가 음수이면 쉼표
	if (curMusic[curNoteIdx].loopCnt == 0 && status == STAT_PLAY) {
			buzStatus = !buzStatus;
			if (buzStatus) {
				PORTB = 0x10;
			}
			else {
				PORTB = 0x00;
			}
			TCNT2 = curMusic[curNoteIdx].remainCnt;
	}
	else if (curMusic[curNoteIdx].loopCnt > 0 && status == STAT_PLAY) {
		curOvfCnt++;

		if (compLoop) {
			compLoop = FALSE;
			buzStatus = !buzStatus;
			if (buzStatus) {
				PORTB = 0x10;
			}
			else {
				PORTB = 0x00;
			}
			curOvfCnt = 0;
		}
		else if (curOvfCnt >= curMusic[curNoteIdx].loopCnt) {
			compLoop = TRUE;
			TCNT2 = curMusic[curNoteIdx].remainCnt;
		}
	}
}

// 함수 구현부
void initRegister(void)
{
	OS_ENTER_CRITICAL();
	// LED
	DDRA = 0xff;

	// 패시브 부저 관련 설정
	// 부저의 연주는 타이머2의 오버플로우 인터럽트를 사용한다.
	DDRB = 0x10; // Passivee Buzzer
	TCCR2 = 0x03;
	TIMSK |= 0x40;
	// TCNT2 = DO_DATA;

	// FND 관련 레지스터 설정
	DDRC = 0xff;	// FND
	DDRG = 0x0f;	// FND Select
	
	// 외부 인터럽트 관련 레지스터 설정
	DDRE = 0xcf;	// SW1(PE4), SW2(PE5): input
	EICRB = 0x0a;	// INT4,5: Falling edge
	EIMSK = 0x30;	// INT4,5 Enable

	// 전역 인터럽트 허용 (SREG bit7)
	sei();
	OS_EXIT_CRITICAL();
}

void randTask(void *pdata)
{
	INT8U err;
	INT8U randNum;
	INT8U oldRandNum = 0;
	pdata = pdata;

	OSStatInit();
	OSTimeDlyHMSM(0, 0, 0, 100);

	while (1) {
		OSFlagPost(flagState, 0xff, OS_FLAG_CLR, &err);

		while ((randNum = rand() % N_MUSICS) == oldRandNum); // 이전 값과 중복을 방지한다.
		OSMboxPost(msgRand, (void*)&randNum);
		oldRandNum = randNum;
		OSFlagPost(flagState, 0x01, OS_FLAG_SET, &err);

		OSFlagPend(flagState, 0x02, OS_FLAG_WAIT_SET_ALL, 0, &err); // 음악의 재생이 끝날 때까지 기다린다.
		OSTimeDlyHMSM(0, 0, 1, 0);
	}
}

void musicTask(void *pdata)
{
	INT8U err;
	INT8U *msg;
	pdata = pdata;

	curNoteIdx = 0;

	while (1) {
		OSFlagPend(flagState, 0x01, OS_FLAG_WAIT_SET_ALL, 0, &err);
		PORTA = 0xff;
		msg = (INT8U*)OSMboxPend(msgRand, 0, &err);

		curMusicIdx = *msg;
		curMusic = musicList[curMusicIdx];

		PORTA = 1 << curMusicIdx;
		status = STAT_PLAY;
		curNoteIdx = 0;
		
		// 음악 재생
		while (curNoteIdx < noteSizes[curMusicIdx]) {
			if (curMusic[curNoteIdx].loopCnt == 0)
				TCNT2 = curMusic[curNoteIdx].remainCnt;
			OSTimeDlyHMSM(0, 0, curMusic[curNoteIdx].length / 1000, curMusic[curNoteIdx].length % 1000);
			if (status == STAT_PLAY)
				curNoteIdx++;
		}

		status = STAT_STOP;
		OSFlagPost(flagState, 0x02, OS_FLAG_SET, &err); // 음악 재생이 끝나면 이벤트 플래그를 설정하여 종료 여부를 알린다.
		OSTimeDlyHMSM(0, 0, 1, 0);
	}
}

// 현재 상태 단순 출력 기능
void fndTask(void *pdata)
{
	INT8U err;
	INT8U i;
	INT8U fndvalue[4];
	pdata = pdata;

	OSFlagPend(flagState, 0x01, OS_FLAG_WAIT_SET_ALL, 0, &err);

	while (1) {
		switch (status) {
		case STAT_STOP:
			fndvalue[3] = fnd_alpha[5];
			fndvalue[2] = fnd_alpha[6];
			fndvalue[1] = fnd_alpha[7];
			fndvalue[0] = fnd_alpha[0];
			break;
		case STAT_PLAY:
			fndvalue[3] = fnd_alpha[0];
			fndvalue[2] = fnd_alpha[1];
			fndvalue[1] = fnd_alpha[2];
			fndvalue[0] = fnd_alpha[3];
			break;
		case STAT_PAUSE:
			fndvalue[3] = fnd_alpha[0];
			fndvalue[2] = fnd_alpha[2];
			fndvalue[1] = fnd_alpha[4];
			fndvalue[0] = fnd_alpha[5];
			break;
		}

		for (i = 0; i < 4; i++) {
			PORTG = fnd_sel[i];
			PORTC = fndvalue[i];
			OSTimeDlyHMSM(0, 0, 0, 2);
		}
	}
}

int main(void)
{
	INT8U err;
	OSInit();

	OS_ENTER_CRITICAL();
	// 타이머0은 uC/OS-II 운영체제에서 사용된다.
	TCCR0 = 0x07;	// 1024 분주비
	TIMSK = _BV(TOIE0);
	TCNT0 = 256 - (CPU_CLOCK_HZ / OS_TICKS_PER_SEC / 1024);
	OS_EXIT_CRITICAL();

	initRegister();

	PORTA = 0x00;
	PORTG = 0x0f;
	PORTC = 0x00;

	status = STAT_STOP;

	msgRand = OSMboxCreate(NULL); // 메일박스 생성
	flagState = OSFlagCreate(0x00, &err); // 이벤트 플래그 생성

	OSTaskCreate(randTask, (void*)NULL, (void*)&taskStk[0][TASK_STK_SIZE - 1], 1);
	OSTaskCreate(musicTask, (void*)NULL, (void*)&taskStk[1][TASK_STK_SIZE - 1], 2);
	OSTaskCreate(fndTask, (void*)NULL, (void*)&taskStk[2][TASK_STK_SIZE - 1], 3);

	OSStart();

	return 0;
}