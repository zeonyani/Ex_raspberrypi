#include <stdio.h>
#include <stdlib.h> // atoi() 함수
#include <string.h>
#include <fcntl.h>
#include <unistd.h> //close() 함수

int ledControl(int gpio)
{
    int fd;
    char buf[BUFSIZ];

    fd = open("/sys/class/gpio/export", O_WRONLY); // 해당 gpio 디바이스 사용준비
    sprintf(buf, "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);

    sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio); // 해당 gpio 디바이스 방향 설정
    fd = open(buf, O_WRONLY);
    write(fd, "out", 4);
    close(fd);

    sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio); // 디바이스에 값출력
    fd = open(buf, O_WRONLY);
    write(fd, "1", 2);
    close(fd);

    getchar(); // LED확인을 위한 대기
	// LED 끄기
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);   
	fd = open(buf, O_WRONLY);
	write(fd, "0", 2);
	close(fd); 
    fd = open("/sys/class/gpio/unexport", O_WRONLY); // 사용한 gpio 디바이스 해체
    sprintf(buf, "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);

    return 0;
}

int main(int argc, char **argv)
{
	int gno;
	if(argc<2){
		printf("Usage: %s GPIO_NO\n", argv[0]);
		return -1;
	}
	gno = atoi(argv[1]);
	ledControl(gno);

	return 0;
} 
