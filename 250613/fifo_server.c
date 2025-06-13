#include <stdio.h>
#include <fcntl.h>
#include <unistd.h> // read(), close(0, unlink() 등의 시스템 콜을 위한 헤더파일
#include <sys/stat.h>

#define FIFOFILE    "fifo"

int main(int argc, char **argv)
{
    int n, fd;
    char buf[BUFSIZ];

    unlink(FIFOFILE); // 기존의 FIFO파일 삭제

    if(mkfifo(FIFOFILE, 0666) < 0) { // 새로운 FIFO파일 생성
        perror("mkfifo()");
        return -1;
    }

    if((fd=open(FIFOFILE, O_RDONLY)) < 0) { // FIFO 열기
        perror("open()");
        return -1;
    }

    while((n=read(fd, buf, sizeof(buf))) >0) // FIFO로부터 데이터 받기
        printf("%s", buf); // 읽어온 데이터를 화면에 출력
    close(fd);
    return 0;
}
