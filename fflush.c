#include <stdio.h>
#include <unistd.h>

int main(void)
{
	printf("Hello~");
	printf("world");
	fflush(stdout);
	_exit(1);
	return 0;
}
