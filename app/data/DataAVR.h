#ifndef __DATA_AVR_H__
#define __DATA_AVR_H__

typedef struct tagNoteDataAVR {
	INT8S loopCnt; // 음 출력을 위한 타이머 오버플로우 횟수
	INT8U remainCnt;  // 루프 이후에 추가적으로 타이머에 들어가는 값 
	INT16U length; // 음의 길이(ms단위)
} NoteDataAVR;

#endif