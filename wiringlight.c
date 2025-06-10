#include <wiringPi.h>
#include <stdio.h>

#define SW 5 // gpio24
#define CDS 0 // gpio17
#define LED 1 // gpio18

int cdsControl()
{
	int i;
	pinMode(SW, INPUT); // 핀모드-입력으로 설정
	pinMode(CDS, INPUT); // 핀모드-입력으로 설정
	pinMode(LED, OUTPUT); // 핀모드-출력으로 설정

	for(;;){
		if(digitalRead(CDS) == HIGH){ // 조도 센서 검사를 위한 무한루프
		digitalWrite(LED, HIGH); // 빛이 감지되면
		delay(1000);
		digitalWrite(LED, LOW); // 끄기
		}
	}
	return 0;
}

int main(void)
{
	wiringPiSetup();
	cdsControl(); // 조도센서 사용을 위한 함수 호출
	return 0;
}
