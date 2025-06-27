#include <stdio.h> // printf, perror 등
#include <string.h> // exit 등
#include <unistd.h> // close, read, write 등
#include <string.h> // memset, strlen 등
#include <sys/socket.h> // socket, connect 등
#include <arpa/inet.h> // inet_pton 등
#include <errno.h> // errno 변수
#include <fcntl.h> // fcntl, O_NONBLOCK

#define TCP_PORT 5100
#define BUFSIZE 1024
#define NICKNAME_MAX_LEN 31 // 닉네임 최대 길이(널문자 포함 32바이트)

// 업로드 되는지 확인하기 위한 줄

int main(int argc, char **argv)
{
    int ssock; // 서버와의 통신 소켓
    struct sockaddr_in servaddr;
    char buffer[BUFSIZ];
    int nbytes;
    char nickname[NICKNAME_MAX_LEN + 1]; // 닉네임 버퍼

    if(argc<2) {
        printf("Usage : %s IP_ADRESS\n", argv[0]);
        return -1;
    }

    // 1. 소켓을 생성
    if((ssock=socket(AF_INET, SOCK_STREAM, 0)) <0){
        perror("socket()");
        return -1;
    }

    // 2. 소켓이 접속할 주소 지정
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    // 문자열을 네트워크 주소로 변경
    inet_pton(AF_INET, argv[1], &(servaddr.sin_addr.s_addr));
    servaddr.sin_port = htons(TCP_PORT);

    // 3. 지정한 주소로 접속
    if(connect(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) <0){
        perror("connect()");
        close(ssock);
        return -1;
    }
    printf("Connect to server.\n");

    // 닉네임 입력 요청 및 전송
    printf("Enter your nickname(max: %d chars): ", NICKNAME_MAX_LEN);
    if(fgets(nickname, NICKNAME_MAX_LEN + 1, stdin) == NULL) {
        perror("fgets nickname");
        close(ssock);
        return -1;
    }

    // 개행 문자 제거
    nickname[strcspn(nickname, "\n")] = 0;

    // 닉네임이 비어있으면 기본값 설정 (에러처리)
    if(strlen(nickname) == 0) {
        strcpy(nickname, "Guest: ");
        printf("Nickname can't be empty. You're 'Guest'.\n");
    }

    // 닉네임 전송
    if(write(ssock, nickname, strlen(nickname)) == -1){
        perror("wirte() nickname to server error");
        close(ssock);
        return -1;
    }
    printf("Nickname %s sent to sever.\n", nickname);
    // 여기까지가 닉네임 기능

    // 4. 표준 입력(stdin)과 서버 소켓을 논블로킹 모드로 설정 (동시 처리를 위함)
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // 표준 입력 (파일디스크립터 0)
    fcntl(ssock, F_SETFL, O_NONBLOCK); // 서버 소켓

    // 5. 연속적인 메시지 송수신 루프
    printf("Enter mnessage('q' to quit): \n");
    while(1) {
        // (A) 표준 입력에서 메시지 읽기 (사용자가 입력한 메시지)
        nbytes = read(STDIN_FILENO, buffer, BUFSIZE-1);
        if(nbytes > 0){
            buffer[nbytes] = '\0'; // 널 종료 문자 추가
            // 세 줄 문자 제거 (read는 '\n'을 포함하므로 제거 필요)
            if(buffer[nbytes-1] == '\n'){
                buffer[nbytes-1] = '\0';
                nbytes--; // 길이를 줄임
            }

            if(strcmp(buffer, "q") == 0) { // 'q'입력시 종료
                printf("Disconnecting . . .\n");
                break;
            }

            // 메시지를 서버로 전송(닉네임은 서버가 붙여줌
            if(write(ssock, buffer, nbytes) == -1){
                perror("write() to server error");
                break;
            }
        } else if(nbytes == -1 && errno != EWOULDBLOCK) {
            // EWOULDBLOCK은 데이터가 없을 때 발생하는 정상 상태(논블로킹) 그 외 오류
            perror("read from stdin error");
            break;
        }

        // (B) 서버로부터 메시지 읽기
        nbytes = read(ssock, buffer, BUFSIZE-1);
        if(nbytes > 0) {
            buffer[nbytes] = '\0'; // 널 종료 문자 추가
            printf("%s\n", buffer); // 서버로부터 받은 메시지 출력
        } else if(nbytes == 0) { // 서버 연결 종료
            printf("Disconnected from server.\n");
            break;
        } else if(nbytes == -1 && errno != EWOULDBLOCK) {
            perror("read from server error");
            break;
        }

        // CPU 과사용 방지를 위한 짧은 지연
        usleep(10000); // 10ms
    }

     // 6. 소켓 닫기
    close(ssock);
    printf("Client terminated.\n");
    return 0;
}
