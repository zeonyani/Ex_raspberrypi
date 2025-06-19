#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MSG_SIZE 100

int main(void)
{
    int a_to_parent[2], parent_to_b[2]; // 자식A->부모
    int b_to_parent[2], parent_to_a[2]; // 부모->자식B

    pipe(b_to_parent); // 자식B->부모
    pipe(parent_to_a); // 부모->자식A

    pid_t pid_a = fork();

    if(pid_a==0){
        
        char reply[MSG_SIZE]; // 받아올 reply 배열
        char buffer[MSG_SIZE]; // 읽어올 buffer 배열

        // 자식A 프로세스 - 클라이언트A
        close(a_to_parent[0]); // 안 쓰는 읽기 끝
        close(parent_to_a[1]); // 안 쓰는 쓰기 끝

        while(1) {
            fgets(reply, MSG_SIZE, stdin); // 사용자 입력
            write(a_to_parent[1], reply, strlen(reply)+1); // 부모에게 쓰기
            read(parent_to_a[0], buffer, MSG_SIZE); // 부모로부터 수신
            printf("수신 : %s", buffer); // 출력
        }

        return 0;
    }

    pid_t pid_b = fork();

    if(pid_b==0){
        
        char reply[MSG_SIZE]; // 받아올 reply 배열
        char buffer[MSG_SIZE]; // 읽어올 buffer 배열
        
        // 자식B - 클라이언트B
        close(b_to_parent[0]); // 안 쓰느 읽기 끝
        close(parent_to_b[1]); // 안 쓰는 쓰기 끝

        while(1) {
            fgets(reply, MSG_SIZE, stdin); // 사용자 입력
            write(a_to_parent[1], reply, strlen(reply)+1); // 부모에게 쓰기
            read(parent_to_a[0], buffer, MSG_SIZE); // 부모로부터 수신
            printf("수신 : %s", buffer); // 출력
        }

        return 0;
    }

    // 부모 프로세스 -> 서버
    close(a_to_parent[1]); // 읽기만 함
    close(parent_to_a[0]); // 쓰기만 함
    close(b_to_parent[1]); // 읽기만 함
    close(parent_to_b[0]); // 쓰기만 함

    while(1){
        char buf[MSG_SIZE];

        // A가 부모에게 보낸 메시지를 읽고 B에게 전달
        if(read(a_to_parent[0], buf, MSG_SIZE)>0) write(parent_to_b[1], buf, strlen(buf)+1);

        // B가 부모에게 보낸 메시지를 읽고 A에게 전달
        if(read(b_to_parent[0], buf, MSG_SIZE)>0) write(parent_to_a[1], buf, strlen(buf)+1);
    }
    return 0;
}
        
