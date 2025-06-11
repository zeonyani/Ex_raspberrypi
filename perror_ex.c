#include <stdio.h>

int main(void)
{
	char str[BUFSIZ], out[BUFSIZ];
	
	scanf("%s", str); // 사용자로부터 문자열 입력받기
	printf("1 : %s\n", str); // 입력된 문자열 출력
	sprintf(out, "2 : %s\n", str); // out에 문자열을 포맷팅해서 저장
	perror(out);

	return 0;
}

