#include <wiringPi.h>
#include <softTone.h>

#define SPKR 6
#define TOTAL 42

int notes[]={
	262, 262, 391, 391, 440, 440, 391,\
	349, 349, 330, 330, 294, 294, 262,\
	391, 391, 349, 349, 330, 330, 293,\
	391, 391, 349, 349, 330, 330, 293,\
	262, 262, 391, 391, 440, 440, 391,\
	349, 349, 330, 330, 294, 294, 262
};

int musicPlay()
{
	int i;
	softToneCreate(SPKR);
	for(i=0;i<TOTAL;i++){
		softToneWrite(SPKR, notes[i]);
		delay(280);
	}
	return 0;
}

int main(void)
{
	wiringPiSetup();
	musicPlay();
	return 0;
}
