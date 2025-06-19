#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

int main(void)
{
    int pipe1[2]; // 부모->자식
    int pipe2[2]; // 자식->부모

    pid_t pid;

    // 파이프 생성
    if(pipe(pipe1)==-1||pipe(pipe2)==-1){
        perror("pipe failed");
        return 1;
    }
    pid = fork();

    if(pid<0){
        perror("fork failed");
        return 1;
    }else if(pid==0){ // 자식 프로세스
        // 부모->자식 읽기용
        close(pipe1[1]); // 자식은 pipe1의 쓰기 끝 안씀
        
        // 자식->부모 쓰기용
        close(pipe2[0]); // 자식은 pipe2의 읽기 끝 안씀

        char buffer[100];
        read(pipe1[0], buffer, sizeof(buffer));
        printf("자식이 받은 메시지 : %s\n", buffer);

        // 자식->부모 응답
        char* reply = "자식이 보내는 메시지!";
        write(pipe2[1], reply, strlen(reply)+1);

        // 파이프 닫기
        close(pipe1[0]);
        close(pipe2[1]);
    }else{ // 부모 프로세스
        // 부모->자식 쓰기용
        close(pipe1[0]); // 부모는 pipe1의 읽기 끝 안씀
        // 자식->부모 읽기용
        close(pipe2[1]); // 부모는 pipe2의 쓰기 끝 안씀

        // 부모->자식 전송
        char* msg = "부모가 보내는 메시지~";
        write(pipe1[1], msg, strlen(msg)+1);

        // 자식 응답 받기
        char  buffer[100];
        read(pipe2[0], buffer, sizeof(buffer));
        printf("부모가 받은 메시지: %s\n", buffer);
        
        // 파이프 닫기
        close(pipe1[1]);
        close(pipe2[0]);
    }
    return 0;
}
