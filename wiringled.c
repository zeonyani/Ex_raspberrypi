#include <wiringPi.h> // wiringPi 라이브러리를 위한 헤더파일
#include <stdio.h>
#include <stdlib.h>

int ledControl(int gpio)
{
	int i;
	
	pinMode(gpio, OUTPUT); // Pin출력 설정

	for(i=0;i<5;i++){
		digitalWrite(gpio, HIGH); // HIGH(1)값 출력: LED켜기
		delay(1000); // 1초 동안 대기
		digitalWrite(gpio, LOW); // LOW(0)값 출력 : LED끄기
		delay(1000);
	}

	return 0;
}

int main(int argc, char **argv)
{
	int gno;

	if(argc<2){
		printf("Usage : %s GPIO_NO\n", argv[0]);
 		return -1;
	}
	gno = atoi(argv[1]);
	wiringPiSetup(); // wiringPi 초기화
	ledControl(gno);

	return 0;
}	
