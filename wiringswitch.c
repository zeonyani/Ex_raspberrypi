#include <wiringPi.h>
#include <stdio.h>

#define SW 5 // gpio 24
#define LED 1 // gpio 18

int switchControl()
{
	int i;
	pinMode(SW, INPUT); // Pin모드를 입력으로 설정
	pinMode(LED, OUTPUT);

	for(;;) { // 스위치 확인을 위한 무한루프
		if(digitalRead(SW) == LOW) // 버튼 눌리면
		{digitalWrite(LED, HIGH); // 켜기
		delay(1000);
		digitalWrite(LED, LOW); // 끄기 
		}
	delay(10); // 이벤트 감지를 위한 쉼(ctrl+c)
	}
	return 0;
}

int main(int argc, char **argv)
{
	wiringPiSetup(); // 스위치 사용을 위한 함수
	switchControl();
	return 0;
}
