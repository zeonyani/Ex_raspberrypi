#include <stdio.h>
#include <wiringPi.h>

int main(void)
{
	int i, a, b;
	int seg7[10][7] = {	{1,1,1,1,1,1,0},
				{0,1,1,0,0,0,0},
				{0,0,1,0,0,1,0},
				{0,0,0,0,1,1,0},
				{1,0,0,1,1,0,0},
				{0,1,0,0,1,0,0},
				{0,1,0,0,0,0,0},
				{0,0,0,1,1,1,1},
				{0,0,0,0,0,0,0},
				{0,0,0,0,1,0,0}};

	wiringPiSetup();

	for(i=0;i<7;i++) pinMode(i, OUTPUT);

	while(1) {
		for(a=0;a<10;a++){
			for(b=0;b<7;b++) digitalWrite(b, seg7[a][b]);
			delay(1000);
		}
	}
	return 0;
}
