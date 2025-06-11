#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
	
	int n;
	int fd;

	scanf("%d", &n);

	printf("Hello world(%d)\n", n);
	fd = open("sample250611.txt", O_WRONLY | O_CREAT, 0644);

	if(fd==-1){
		perror("failed");
		return 1;
	}

	printf("\n\n\n\n%d\n", fd);

	close(fd);

	return 0;
}
