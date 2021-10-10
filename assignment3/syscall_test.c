#include <stdio.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
int main(){
	while(1){
		int a, b, op, ans, len, idx = 0;
		char expr[64], buf[64];
		memset(buf, '\0', sizeof(buf));
		printf(">> ");
		scanf("%s", expr);
		len = strlen(expr);
		if(expr[0] == 'q') break;
		for(int i=0; i<len; ++i){
			if(expr[i]=='+' || expr[i]=='-' || expr[i]=='*' || expr[i]=='%'){
				if(expr[i] == '+') op = 442; // system call 번호 설정
                	        else if(expr[i] == '-'){
					if(idx == 0){ // 가장 앞에 등장하는 '-'기호는 숫자에 포함시킴
						buf[idx] = expr[i];
						idx++;
						continue;
					}
					op = 443;
				}
                        	else if(expr[i] == '*') op = 444;
                          	else if(expr[i] == '%') op = 445;
				a = atoi(buf); // 첫번째 수
				memset(buf, '\0', sizeof(buf));
				idx = 0;
			}
			else if(isdigit(expr[i])){ // 숫자 생성
				buf[idx] = expr[i];
				idx++;
			}
			else{
				printf("Unexpected token\n");
				exit(-1);
			}
		}
		b = atoi(buf); // 두번째 수
		long ret = syscall(op, a, b, &ans); // 미리 등록한 번호에 따라 system call 호출
		if(ret == -1){
			fprintf(stderr, "error occurred while copying block data\n");
			exit(-1);
		}
		printf("ans = %d\n", ans);
	}
	return 0;
}
