#include <stdio.h> // printf, perror 등 기본 입출력
#include <stdlib.h> // exit, system 등
#include <unistd.h> // fork, close, read, write, setsid 등 POSIX API
#include <string.h> // memset, strlen, strcmp 등 문자열 처리
#include <signal.h> // signal, sigaction 등 시그널 처리
#include <syslog.h> // openlog, syslog, closelog 등 시스템 로그
#include <sys/stat.h> // umask
#include <sys/types.h> // pid_t
#include <sys/socket.h> // socket, bind, listen, accept 등 소켓 API
#include <arpa/inet.h> // htonl, htons, inet_ntoa 등 네트워크 주소 변환
#include <netinet/in.h> // sockaddr_int 구조체
#include <errno.h> // errno 변수
#include <sys/resource.h> // getrlimit, setrlimt (파일 디스크립터 제한 설정)
#include <sys/wait.h> // waitpid()
#include <fcntl.h> // open(), O_RDWR, fcntl(), O_NONBLOCK

#define TCP_PORT 5100 // 서버가 사용할 포트 번호
#define BUFSIZE 1024 // 버퍼 크기
#define MAX_CLIENTS 10 // 최대 클라이언트 수(조절 가능)

// 전역 변수 및 시그널 핸들러 선언
typedef struct {
    pid_t pid;
    int parent_read_pipe_fd; // 부모가 자식으로부터 읽을 파이프 FD (자식이 쓸 FD)
    int child_write_pipe_fd; // 자식이 부모에게 쓸 파이프 FD (부모가 읽을 FD)
    char client_ip[INET_ADDRSTRLEN]; // 클라이언트 IP 주소
    int client_port; // 클라이언트 포트 번호 (추가 정보)
    char nickname[32]; // 클라이언트 닉네임
    int is_active; // 해당 슬롯이 활성상태인지 여부(1: 활성 0: 비활성)
}client_info_t;

// 활성 클라이언트(자식 프로세스) 정보를 저장할 배열
client_info_t clients[MAX_CLIENTS];

// 클라이언트 정보 배열 초기화 함수
void init_clients_array(){
    for(int i = 0; i < MAX_CLIENTS; i++){
        clients[i].pid = 0; // 0은 사용하지 않는 PID
        clients[i].parent_read_pipe_fd = -1;
        clients[i].child_write_pipe_fd = -1;
        memset(clients[i].client_ip, 0, sizeof(clients[i].client_ip));
        clients[i].client_port = 0;
        memset(clients[i].nickname, 0, sizeof(clients[i].nickname));
        clients[i].is_active = 0; // 비활성 상태로 초기화
     }
     syslog(LOG_INFO, "Clients array initialized.");
}

volatile sig_atomic_t server_running = 1; // 서버 종료를 위한 플래그
volatile sig_atomic_t sigusr1_received = 0; // SIGUSR1 수신 플래그 (부모->자식 메시지 알림)
volatile sig_atomic_t sigusr2_received = 0; // SIGUSR2 수신 플래그 (자식->부모 메시지 알림)

// SIGCHILD 시그널 핸들러 : 좀비 프로세스 방지
void handle_sigchld(int sig){
    int status;
    pid_t pid;

    // waitpid를 사용하여 종료된 자식 프로세스의 자원을 회수
    while((pid = waitpid(-1, &status, WNOHANG)) > 0 ){
        syslog(LOG_INFO, "Child process %d terminated.(status: %d)", pid, status);

        // clients 배열에서 해당 PID 정보 제거
        for(int i = 0; i < MAX_CLIENTS; i++){
            if(clients[i].is_active && clients[i].pid == pid) {
                syslog(LOG_INFO, "Removing client %d (PID: %d) from active list.", i, pid);
                if(clients[i].parent_read_pipe_fd != -1){
                    close(clients[i].parent_read_pipe_fd);
                    syslog(LOG_INFO, "Closed parent_read_pipe_fd %d for PID %d.", clients[i].parent_read_pipe_fd, pid);
                }
                if(clients[i].child_write_pipe_fd != -1){
                    close(clients[i].child_write_pipe_fd);
                    syslog(LOG_INFO, "Close child_write_pipe_fd %d for PID %d", clients[i].child_write_pipe_fd);
                }
                clients[i].is_active = 0; // 슬롯 비활성화
                clients[i].pid = 0; // PID 초기화
                memset(clients[i].client_ip, 0, sizeof(clients[i].client_ip));
                clients[i].client_port = 0;
                memset(clients[i].nickname, 0, sizeof(clients[i].nickname));
                break;
            }
        }
    }
}

// SIGINT (ctrl+c) 및 SIGCTERM 시그널 핸들러 : 서버 우아한 종료
void handle_sigint_term(int sig){
    syslog(LOG_INFO, "SIGINT/SIGTERM received. Initiation graceful shutdown.");
    server_running = 0; // 메인 루프 종료 플래그 설정
}

// SIGUSR1 시그널 핸들러 (부모->자식 / 새로운 메시지 도착 알림)
void handle_sigusr1(int sig){
    // syslog(LOG_INFO, "SIGUSR1 received. (This indicates a new message is ready for IPC)"); (4단계에서 수정)
    sigusr1_received = 1;
}

// SIGUSR2 시그널 핸들러 (자식->부모 / 클라이언트로부터 메시지 수신했음 알림)
void handle_sigusr2(int sig){
    // syslog(LOG_INFO, "SIGUSR2 received. (This indicates a client message for broadcast)"); (4단계에서 수정)
    sigusr2_received = 1;
    // 시그널 핸들러 내 getpid()는 안전하나 누가 보냈는지 정확 식별 곤란
    // 일단 플래그만 설정하고 부모 루프에서 모든 파이프를 확인하여 메시지 읽음
    // 추후 누가 보냈는지 특정하는 더 좋은 방법을 고려할 수 있음
}

// 클라이언트와 통신을 전달하는 자식 프로세스용 함수
void handle_client_communication(int csock, int to_parent_pipe_wfd, int from_parent_pipe_rfd){
    char buffer[BUFSIZE];
    int nbytes;

    // 파이프 FD들을 논블로킹 모드로 설정 (select/epoll 사용금지이므로 필수!)
    // O_NOONBLOCK은 fcntl.h에 정의
    fcntl(to_parent_pipe_wfd, F_SETFL, O_NONBLOCK);
    fcntl(from_parent_pipe_rfd, F_SETFL, O_NONBLOCK);
    fcntl(csock, F_SETFL, O_NONBLOCK); // 클라이언트 소켓도 논블로킹

    syslog(LOG_INFO, "Child %d communication handler started.", getpid());

    while(1){
        // (1) 클라이언트로부터 메시지 읽기
        nbytes = read(csock, buffer, BUFSIZE-1);
        if(nbytes > 0) {
            buffer[nbytes] = '\0';
            syslog(LOG_INFO, "Child %d received from client: %s", getpid(), buffer);
            if(strcmp(buffer, "q") == 0){
                syslog(LOG_INFO, "Child %d client sent 'q', breaking communication loop.", getpid());
                break; // 'q'입력 시 종료
            }

            // 클라이언트로부터 받은 메시지를 부모에게 파이프로 전송하고 SIGUSR2 시그널 전송
            if(write(to_parent_pipe_wfd, buffer, nbytes) == -1) {
                if(errno == EPIPE){ // 파이프가 닫혔을 경우 (부모가 종료됐을 수 있음)
                    syslog(LOG_WARNING, "Child %d: Broken pipe to parent, parent might have terminated.", getpid());
                    break; // 통신 루프 종료
                } else if(errno != EWOULDBLOCK){
                    syslog(LOG_ERR, "Child %d write to parent pipe error: %m", getpid());
                    break; // 통신 루프 종료
                }
            } else{
                syslog(LOG_INFO, "Child %d sent message to parent via pipe.", getpid());
                // 부모에게 SIGUSR2 시그널 전송하여 메시지 도착 알림
                kill(getppid(), SIGUSR2);
            }
        } else if(nbytes == 0){ // 클라이언트가 연결을 정상적으로 종료한 경우
                syslog(LOG_ERR, "Child %d read from client error: %m", getpid());
                break; // 통신 루프 종료
        } else if(nbytes == -1 && errno != EWOULDBLOCK) { // 에러 - EWOULDBLOCK은 논블로킹일 때 데이터 없음
            syslog(LOG_ERR, "Child %d read from client error: %m", getpid());
            break; // 통신 루프 종료
        }

        // (2) 부모로부터 파이프를 통해 메시지 읽기 (SIGUSR1 받으면 처리)
        if(sigusr1_received){ // SIGUSR1 시그널이 발생했을 경우
            sigusr1_received = 0; // 플래그 초기화
            // 파이프로부터 메시지 읽기
            nbytes = read(from_parent_pipe_rfd, buffer, BUFSIZE - 1);
            if(nbytes > 0){
                buffer[nbytes] = '\0';
                syslog(LOG_INFO, "Child %d received from parent via pipe: %s", getpid(), buffer);
                if(write(csock, buffer, nbytes) == -1) {
                    syslog(LOG_ERR, "Child %d write to client error : %m", getpid());
                }
            } else if(nbytes == 0){ // 파이프 닫힘(부모가 종료 or 파이프 닫음)
                syslog(LOG_INFO, "Child %d read from parent pipe error: %m", getpid());
                break;
            } else if(nbytes == -1 && errno != EWOULDBLOCK) { // 논블로킹 읽기 오류 처리
                syslog(LOG_ERR, "Child %d read from parent pipe error: %m", getpid());
                break;
            }
        }

        // 짧은 지연 (CPU 낭비 방지)
        usleep(10000); // 10ms
    }
    syslog(LOG_INFO, "Child %d communication handler finished.", getpid());
}

int main(int argc, char **argv)
{
    int ssock, csock; // 서버 소켓, 클라이언트 소켓
    struct sockaddr_in servaddr, cliaddr; // 서버 주소, 클라이언트 주소 구조체
    socklen_t clen = sizeof(cliaddr); // 클라이언트 주소 구조체 크기
    pid_t pid; // fork()를 위한 PID

    // 0. TCP 서버 소켓 생성 및 바인딩 (데몬화 이전 수행)
    // 0-1. 소켓 생성 (IPv4, TCP 스트림 소켓)
    ssock = socket(AF_INET, SOCK_STREAM, 0);
    if(ssock == -1){
        perror("socket()"); // syslog는 아직 초기화되지 않을 수 있어서 perror 사용
        exit(EXIT_FAILURE);
    }

    int optval = 1; // SO_REUSEADDR은 나중에 바인딩 문제가 없도록 미리 설정
    if(setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt(SO_REUSEADDR)");
        close(ssock);
        exit(EXIT_FAILURE);
    }

    // 1. 데몬화 로직
    // 1-1. 파일 생성 마스크를 0으로 설정 (모든 권한 허용)
    umask(0);

    // 1-2. 프로세스를 포크하여 부모 프로세스 종료
    // 부모 프로세스가 종료되면 쉘은 즉시 프롬프트 반환
    pid = fork();
    if(pid < 0){ // fork 실패
        perror("fork()");
        exit(EXIT_FAILURE);
    }
    if(pid > 0) { // 부모 프로세스: 즉시 종료
        exit(EXIT_SUCCESS);
    }

    // 1-3. 새 세션 리더가 되어 터미널 제어를 상실
    // 데몬은 터미널과 독립적으로 실행
    if(setsid() < 0){
        perror("setsid()");
        exit(EXIT_FAILURE);
    }

    // 1-4. 시그널 처리: SIG_IGN(무시) 설정
    // 터미널 관련 시그널(SIGHUP) 무시
    signal(SIGHUP, SIG_IGN); // SIGHUP 시그널 무시

    // 1-5. 다시 포크하여 세션 리더가 데몬이 되지 않도록 함
    // 이렇게 하면 프로세스가 터미널에서 완전히 분리
    pid = fork();
    if(pid < 0){
        perror("fork()");
        exit(EXIT_FAILURE);
    }
    if(pid > 0) { // 부모(세션 리더) 프로세스: 즉시 종료
        exit(EXIT_SUCCESS);
    }

    // 1-6. 작업 디렉토리를 루트('/')로 변경
    // 이렇게 하면 데몬이 파일 시스템의 특정 지점에 묶이지 않음
    if(chdir("/") < 0){
        perror("chdir()");
        exit(EXIT_FAILURE);
    }

    // 1-7. 모든 파일 디스크립터 닫기
    // 표준 입출력(0, 1, 2) 포함 모든 열림 파일 디스크립터를 닫음
    struct rlimit rl;
    if(getrlimit(RLIMIT_NOFILE, &rl) < 0) { // 시스템의 파일 디스크립터 제한 열기
        perror("getrlimt()");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < rl.rlim_cur; i++){
        // ssock은 닫지 않도록 함
        if(i == ssock) { // ssock은 닫지 않음
            continue;
        }
        close(i);
    }

    // 1-8. 표준 입출력 (stdin, stdout, stderr) /dev/null로 리디렉션
    // 데몬은 터미널 출력을 하지 않으므로 /dev/null 로 보냄
    int fd0 = open("/dev/null", O_RDWR); // stdin   (0)
    int fd1 = dup(0); // stdout (1)
    int fd2 = dup(0); // stderr (2)

    // 파일 디스크립터가 0, 1, 2에 할당되었는지 확인
    if(fd0 != 0 || fd1 != 1 || fd2 != 2){ // 0, 1,2가 아닐 경우 에러
        fprintf(stderr, "Error: unexpected file descriptors %d %d %d\n", fd0, fd1, fd2);
        exit(EXIT_FAILURE);
    }

    // 1-9. 시스템 로그 초기화
    // 데몬은 syslog를 통해 메시지를 기록
    openlog("ChatServerDaemon", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
    syslog(LOG_INFO, "Chat Server Daemon started.");

    init_clients_array(); // 3단계에서 추가 (데몬화 후 clients 배열 초기화)

    // 2. 시그널 핸들러 등록
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); // sigaction 구조체 초기화

    // SIGCHLD 핸들러 등록
    sa.sa_handler = handle_sigchld; // 핸들러 함수 지정
    sigemptyset(&sa.sa_mask); // 시그널 마스크 초기화 (모든 시그널 허용)
    sa.sa_flags = SA_RESTART | SA_NOCLDWAIT; // SA_RESTART: 시스템 호출 재시작, SA_NOCLDWAIT: 좀비 방지 (waitpid와 함께 사용)
    if(sigaction(SIGCHLD, &sa, NULL) == -1){
        syslog(LOG_ERR, "sigaction(SIGCHLD) error: %m");
        exit(EXIT_FAILURE);
    }

    // SIGINT, SIGTERM 핸들러 등록 (Graceful Shutdonw)
    sa.sa_handler = handle_sigint_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // SA_RESTART는 이 경우 불필요
    if(sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "sigaction(SIGINT/SIGTERM) error: %m");
        exit(EXIT_FAILURE);
    }

    // IPC를 위한 SIGUSR1, SIGUSR2 핸들러 등록
    // SIGUSR1 핸들러
    sa.sa_handler = handle_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // 이 시그널 핸들러는 재시작 동작이 필수는 아님
    if(sigaction(SIGUSR1, &sa, NULL) == -1){
        syslog(LOG_ERR, "sigaction(SIGUSR1) error: %m");
        exit(EXIT_FAILURE);
    }

    // SIGUSR2 핸들러
    sa.sa_handler = handle_sigusr2;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // 재시작 동작 필수 아님
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        syslog(LOG_ERR, "sigaction(SIGUSR2) error: %m");
        exit(EXIT_FAILURE);
    }

    // 3. TCP 서버 소켓 바인딩 및 연결 요청 대기열 설정(데몬화 완료 후 실행)
    // 서버 주소 구조체 설정
    memset(&servaddr, 0, sizeof(servaddr)); // 구조체 초기화
    servaddr.sin_family = AF_INET; // IPv4 주소 체계
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 모든 IP 주소에서 접속 허용
    servaddr.sin_port = htons(TCP_PORT); // 포트 번호 설정(네트워크 바이트 순서로 변환)

    // 소켓에 주소 바인딩
    if(bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        syslog(LOG_ERR, "bind() error: %m");
        close(ssock);
        exit(EXIT_FAILURE);
    }

    // 연결 요청 대기열 설정
    if(listen(ssock, 8) == -1){ // 최대 8개의 대기열
        syslog(LOG_ERR, "listen() error: %m");
        close(ssock);
        exit(EXIT_FAILURE);
    }

    // 서버 소켓을 논블로킹 모드로 설정 (새로운 터미널 접속 못하니까 추가)
    fcntl(ssock, F_SETFL, O_NONBLOCK);

    syslog(LOG_INFO, "Chat server listening on port %d...", TCP_PORT);

    // 4. 무한 루프: 클라이언트 연결 수락 및 자식 프로세스 생성, IPC 메시지 처리
    while(server_running) { // 메인 루프 시작
        usleep(10000); // CPU 과사용 방지 및 시그널 처리 기회 제공
        
        // (A) SIGUSR2 시그널 처리 및 메시지 브로드 캐스팅(자식->부모 메시지)
        if(sigusr2_received){
            sigusr2_received = 0; // 플래그 초기화
            syslog(LOG_INFO, "Parent received SIGUSR2. CHecking child pipes for messages.");

            char broadcast_buffer[BUFSIZE];
            int msg_len = 0;
            pid_t sender_pid = 0;

            for(int i = 0; i < MAX_CLIENTS; i++){
                if(clients[i].is_active && clients[i].parent_read_pipe_fd != -1){
                    int n_read = read(clients[i].parent_read_pipe_fd, broadcast_buffer, BUFSIZE-1);
                    if(n_read > 0) {
                        broadcast_buffer[n_read] = '\0';
                        syslog(LOG_INFO, "Parent read '%s' from child %d (PID %d).",
                                broadcast_buffer, i, clients[i].pid);
                        msg_len = n_read;
                        sender_pid = clients[i].pid;
                        break;
                    } else if(n_read == -1 && errno != EWOULDBLOCK) {
                        syslog(LOG_ERR, "Parent read from child pipe %d (PID %d) error: %m",
                                clients[i].parent_read_pipe_fd, clients[i].pid);
                    }
                }
            }

            if(msg_len > 0){
                syslog(LOG_INFO, "Parent broadcasting message : %s (from PID: %d)", broadcast_buffer, sender_pid);
                for(int i = 0; i < MAX_CLIENTS; i++) {
                    if(clients[i].is_active && clients[i].child_write_pipe_fd != -1 && clients[i].pid != sender_pid) {
                        if(write(clients[i].child_write_pipe_fd, broadcast_buffer, msg_len) == -1){
                            if(errno == EPIPE){
                                syslog(LOG_WARNING, "Parent: Broken pipe writing to child %d (PID: %d). Client might have disconnected.",
                                        i, clients[i].pid);
                            } else if(errno != EWOULDBLOCK) {
                                syslog(LOG_ERR, "Parent write to child pipe %d(PID: %d) error: %m",
                                        clients[i].child_write_pipe_fd, clients[i].pid);
                            } else { // 성공적으로 메시지 전송, 자식에게 SIGUSR1 시그널 전송
                                kill(clients[i].pid, SIGUSR1);
                            }
                        }
                    }
                }
            }
        } // sigusr2_received의 끝

        // (B) 새로운 클라이언트 연결 수락 및 자식 프로세스 생성
        // (새로운 터미널이 연결되지 않는 문제에 대한 논리 해결)
        // accept()를 논블로킹 모드로 호출
        csock = accept(ssock, (struct sockaddr *)&cliaddr, &clen);

        if(csock == -1){
            if(errno == EWOULDBLOCK || errno == EAGAIN) {
                // 새로운 연결이 없으면 계속 루프
                usleep(10000); // CPU 과사용 방지
                continue; // 서버가 계속 실행중이면 다시 accept 시도
            } else if(errno == EINTR && !server_running){
                syslog(LOG_INFO, "Parent: accept() interrupted by signal, retrying");
                continue;
            } else if(errno == EINTR && !server_running){
                syslog(LOG_INFO, "Accept interrupted during shutdown");
                break;
            }
            syslog(LOG_ERR, "accept() error: %m. errno: %d", errno); // 추가 터미널 고치려고 errno 확인
            break; // 그 외의 치명적인 에러 시 루프 탈출
        }
        // 클라이언트 연결이 수락되면 (accept() 후, fork() 호출 전
        // clients 배열에서 빈 슬롯을 찾아 파이프를 생성하고 FD 저장하는 로직
        syslog(LOG_INFO, "Clients connected from %s:%d. Seaching for client slot.",
                inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

        // 클라이언트 정보를 저장할 clients 배열의 빈 슬롯 찾기
        int client_idx = -1;
        for(int i = 0; i < MAX_CLIENTS; i++){
            if(clients[i].is_active == 0){ // 비활성 슬롯 발견
                client_idx = i;
                break;
            }
        }

        if(client_idx == -1){ // 사용 가능 슬롯이 없음
            syslog(LOG_WARNING, "Max clients (%d) reached. Rejectin new connection from #s:#d.",
                   MAX_CLIENTS, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
            close(csock);
            continue;
        }

        // 파이프 생성 (부모-자식 간 통신용)
        
        // 클라이언트(자식) -> 부모(메시지 전송용)
        // parent_read_fd[0] : 부모가 읽는 끝
        // parent_read_fd[1] : 자식이 쓰는 끝
        int parent_read_pipe_fds[2];

        // 부모 -> 클라이언트(자식) (메시지 브로드캐스트용)
        // child_write_fd[0] : 자식이 읽는 끝
        // child_write_fd[1] : 부모가 쓰는 끝
        int child_write_pipe_fds[2];

        if(pipe(parent_read_pipe_fds) == -1 || pipe(child_write_pipe_fds) == -1){
            syslog(LOG_ERR, "pipe() creation error: %m");
            close(csock);

            // 파이프 생성 실패 시 열려 있는 FD 닫기
            if(parent_read_pipe_fds[0] != -1) close(parent_read_pipe_fds[0]);
            if(parent_read_pipe_fds[1] != -1) close(parent_read_pipe_fds[1]);
            if(child_write_pipe_fds[0] != -1) close(child_write_pipe_fds[0]);
            if(child_write_pipe_fds[1] != -1) close(child_write_pipe_fds[1]);
            continue;
        }

        pid = fork(); // 클라이언트마다 새로운 프로세스 생성

        if(pid == -1) { // fork 실패
            syslog(LOG_ERR, "fork() error: %m");
            close(csock);
            // 파이프 FD도 닫아주기
            close(parent_read_pipe_fds[0]); close(parent_read_pipe_fds[1]);
            close(child_write_pipe_fds[0]); close(child_write_pipe_fds[1]);
            continue;
        } else if(pid == 0){ // 자식 프로세스
            close(ssock); // 자식은 리스닝 소켓 필요 없음

            // 자식 프로세스에서 파이프 FD 정리
            // 부모가 읽는 파이프의 쓰기 끝 parent_read_pipe_fds[1]은 자식이 사용
            // 부모가 쓰는 파이프의 읽기 끝 child_write_pipe_fds[0]은 자식이 사용
            // 따라서 사용하지 않는 파이프 끝 닫아야 한다
            close(parent_read_pipe_fds[0]); // 부모가 읽는 끝은 자식에게 필요
            close(child_write_pipe_fds[1]); // 부모가 쓰는 끝은 자식에게 필요
            fcntl(parent_read_pipe_fds[1], F_SETFL, O_NONBLOCK); // 자식이 부모에게 스는 파이프(FD: parent_read_pipe_fds[1])
            fcntl(child_write_pipe_fds[0], F_SETFL, O_NONBLOCK); // 자식이 부모로부터 읽는 파이프(FD: child_write_pipe_fds[0])
            syslog(LOG_INFO, "Child process %d handling new client from %s:%d. Assigned pipe FD R:%d W:%d", getpid(),
                   inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port),
                   child_write_pipe_fds[0], parent_read_pipe_fds[1]); // 자식 입장에서의 읽기&쓰기 파이프 FD

            // 클라이언트와 통신하는 함수 (handle_client_communication 호출)
            handle_client_communication(csock, parent_read_pipe_fds[1],
            child_write_pipe_fds[0]);
            // 순서대로 자식이 부모에게 쓰는 파이프 FD
            // 자식이 부모로부터 읽는 파이프 FD 순

            // 통신 완료 후 자식 프로세스에서 모든 FD 닫기
            close(csock);
            close(parent_read_pipe_fds[1]); // 자식이 부모에게 쓴느 파이프 끝 닫기
            close(child_write_pipe_fds[0]); // 자식이 부모로부터 읽는 파이프 끝 닫기
            syslog(LOG_INFO, "Child process %d disconnected from client and exiting", getpid());
            exit(EXIT_SUCCESS); // 자식 프로세스 종료
        } else{ // 부모 프로세스
            close(csock); // 부모는 클라이언트 직접 사용하지 않으므로 닫음

            // 부모 프로세스에서 파이프 FD 정리
            // 부모가 읽는 파이프의 읽기 끝
            // parent_read_pipe_fds[0]은 부모가 사용
            // 부모가 쓰는 파이프의 쓰기 끝
            // child_write_pipe_fds[1]은 부모가 사용
            //
            // 따라서 사용하지 않는 파이프 끝은 닫아야
            close(parent_read_pipe_fds[1]); // 자식이 쓰는 끝은 부모에게 불필요
            close(child_write_pipe_fds[0]); // 자식이 읽는 끝은 부모에게 불필요

            // clients 배열에 자식 프로세스 정보 저장
            clients[client_idx].pid = pid;
            clients[client_idx].parent_read_pipe_fd = parent_read_pipe_fds[0];
            clients[client_idx].child_write_pipe_fd = child_write_pipe_fds[1];
            inet_ntop(AF_INET, &cliaddr.sin_addr, clients[client_idx].client_ip, INET_ADDRSTRLEN);
            clients[client_idx].client_port = ntohs(cliaddr.sin_port);
            clients[client_idx].is_active = 1; // 활성 상태로 설정
            snprintf(clients[client_idx].nickname, sizeof(clients[client_idx].nickname), "user%d", client_idx); // 닉네임 초기 설정

            syslog(LOG_INFO, "Parent process created child %d for client %s:%d at slot %d, Pipes R:%d W:%d",
                    pid, clients[client_idx].client_port, client_idx,
                    clients[client_idx].parent_read_pipe_fd, clients[client_idx].child_write_pipe_fd);
        } // else문의 끝(부모프로세스 블록)
    } // while문의 끝

    // 5. 서버 종료 시 정리 작업
    syslog(LOG_INFO, "Chat SErver Daemin shutting down.");
    // 모든 활성 자식 프로세스 종료 (SIGTERM)
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].is_active){
            syslog(LOG_INFO, "Terminating child process %d (PID: %d).", i, clients[i].pid);
            kill(clients[i].pid, SIGTERM); // 자식에게 종료 시그널 전송
            // 자식 프로세스의 파이프 FD 닫기
            if(clients[i].parent_read_pipe_fd != -1) close(clients[i].parent_read_pipe_fd);
            if(clients[i].child_write_pipe_fd != -1) close(clients[i].child_write_pipe_fd);
        }
    }

    close(ssock); // 서버 리스닝 소켓 닫음
    closelog(); // 시스템 로그 닫음
    return EXIT_SUCCESS;
}
