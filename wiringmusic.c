#include <wiringPi.h>
#include <softTone.h>

#define SPKR	6 // gpio25
#define TOTAL	32 // 학교종 전체 계이름 수

int notes[] = {
	391, 391, 440, 440, 391, 391, 330, 330,\
	391, 391, 330, 330, 294, 294, 294, 0, \
	391, 391, 440, 440, 391, 391, 330, 330, \
	391, 330, 294, 330, 262, 262, 262, 0
};

int musicPlay()
{
	int i;
	softToneCreate(SPKR); // 톤 출력을 위한 gpio 설정
	for(i=0;i<TOTAL;++i){
		softToneWrite(SPKR, notes[i]); // 톤출력
		delay(280); // 음 전체 길이만큼 출력되도록 대기
	}
	return 0;
}

int main()
{
	wiringPiSetup();
	musicPlay(); // 음의 연주를 위한 함수 호출
	return 0;
}
