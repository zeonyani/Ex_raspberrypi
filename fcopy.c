#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define BUF_SIZE 1024

int main(int argc, char **argv)
{
	int fd1, fd2, fd_out, n;
	char buf[BUF_SIZE];

	if(argc!=4){
		fprintf(stderr, "사용법 : fcopy file1 file2\n");
		return 1;
	}

	// file1 열기(읽기전용)
	if((fd1 = open(argv[1], O_RDONLY)) < 0){
		perror("file1 열기 실패");
		return 1;
	}

	// file2 열기(읽기전용)
	if((fd2=open(argv[2], O_RDONLY)) < 0) {
		perror("file2 열기 실패");
		close(fd1);
		return 1;
	}

	// output 파일 열기(쓰기 전용, 없으면 생성, 있으면 덮어씀)
	if((fd_out = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0){
		perror("output 파일 열기 실패");
		close(fd1);
		close(fd2);
		return 1;
	}

	// file1 내용 복사
	while((n=read(fd1, buf, BUF_SIZE)) > 0)
		write(fd_out, buf, n);

	// file2 내용 복사
	while((n=read(fd2, buf, BUF_SIZE)) > 0)
		write(fd_out, buf, n);

	// 파일 닫기
	close(fd1); close(fd2); close(fd_out);
	
	return 0;
}
