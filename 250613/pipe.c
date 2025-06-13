#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h> // waitpid() 함수를 위해 사용

int main(int argc, char **argv)
{
    pid_t pid;
    int pfd[2];
    char line[BUFSIZ]; // stdio.h 파일에 정의된 버퍼 크기로 설정
    int status;

    if(pipe(pfd) < 0) { // pipe함수를 이용해서 파이프 설정
        perror("pipe()");
        return -1;
    }

    if((pid=fork()) <0) { // fork함수를 이용해서 프로세스 생성
        perror("fork()");
        return -1;
    } else if(pid==0) { // 자식 프로세스인 경우의 처리
        close(pfd[0]); // 읽기를 위한 파일 디스크립터 닫기
        dup2(pfd[1],1); // 표준출력(1)을 쓰기 위한 파일 디스크립터(pfd[1])로 변경
        execl("/bin/date", "date", NULL); // date명령어 수행
        close(pfd[1]); // 쓰기를 위한 파일 디스크립터 닫기
        _exit(127);
    } else { // 부모 프로세스인 경우의 처리
        close(pfd[1]); // 쓰기를 위한 파일 디스크립터 닫기
        if(read(pfd[0], line, BUFSIZ) < 0) { // 파일 디스크립터 데이터 읽기
            perror("read()");
            return -1;
        }
        printf("%s", line); // 파일 디스크립터로부터 읽은 내용을 화면에 표시

        close(pfd[0]); // 읽기를 위한 파일 디스크립터 닫기
        waitpid(pid, &status, 0); // 자식 프로세스의 종료를 기다리기
    }
    return 0;
}

/*
    pipe()로 프로세스간 통신을 위한 파이프 생성
    fork()로 부모와 자식프로세스 생성
    자식 프로세스가 date 명령어 실행하고 그 출력이 pfd[1]로 리디렉션
    부모 프로세스는 파이프 읽기(pfd[0])로 자식의 출력을 읽어와서
    부모가 그 결과를 출력
*/

