#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main(void)
{
    pid_t pid;
    
    pid = fork(); // 자식 프로세스 생성

    if(pid<0){
        // fork 실패
        perror("fork failed");
        return 1;
    } else if(pid==0){
        // 자식 프로세스
        printf("Hello from child process! PID: %d\n", getpid());
    } else {
        // 부모 프로세스
        printf("Hello from parent process! PID: %d, Child PID: %d\n", getpid(), pid);
    }

    return 0;
}
