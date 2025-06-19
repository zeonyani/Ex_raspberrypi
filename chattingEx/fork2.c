#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

// 부모와 자식 프로세스가 서로 다르게 동작하도록!

int main(void)
{
    pid_t pid = fork();

    if(pid==0){
        // 자식 프로세스
        for(int i=0;i<5;i++){
            printf("자식 프로세스 : %d\n", i);
            sleep(1);
        }
    }else if(pid>0){
        // 부모 프로세스
        for(int i=0;i<5;i++){
            printf("부모 프로세스 : %d\n", i);
            sleep(1);
        }
    } else{
        perror("fork failed");
        return 1;
    }

    return 0;
}
