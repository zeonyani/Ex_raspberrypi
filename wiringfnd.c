#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>

int fndControl(int num)
{
	int i;
	int gpioins[4] = {4, 1, 16, 14}; // A, B, C, D : 23 18 15 14

	int number[10][4] = {	{0,0,0,0},
				{0,0,0,1},
				{0,0,1,0},
				{0,0,1,1},
				{0,1,0,0},
				{0,1,0,1},
				{0,1,1,0},
				{1,0,0,0},
				{1,0,0,1}	};
	f
