#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFOFILE    "fifo"

int main(int argc, char **argv)
{
    int n, fd;
    char buf[BUFSIZ];

    if((fd=open(FIFOFILE, O_WRONLY)) < 0) { // FIFO 열기
        perror("open()");
        return -1;
    }

    while((n=read(0, buf, sizeof(buf))) > 0) { // 키보드로부터 데이터 입력받기
        write(fd, buf, n); // FIFO로부터 데이터 보내기

    close(fd);
    return 0;
    }
}
