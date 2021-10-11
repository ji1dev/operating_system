#include <stdio.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
int main(){
	while(1){
		int a, b, op = -1, ans, len, flag = 0, idx = 0;
		char expr[64], buf[64];
		memset(buf, '\0', sizeof(buf));
		printf(">> ");
		scanf("%s", expr);
		len = strlen(expr);
		if(expr[0] == 'q') break;
		for(int i=0; i<len; ++i){
			if(expr[i]=='+' || expr[i]=='-' || expr[i]=='*' || expr[i]=='%'){
				if(expr[i] == '+'){
					op = 442; // system call 번호 설정
				}
                	        else if(expr[i] == '-'){
					if(idx == 0){ // 가장 앞에 등장하는 '-'기호는 숫자에 포함시킴
						buf[idx] = expr[i];
						idx++;
						flag = 0;
						continue;
					}
					op = 443;
				}
                        	else if(expr[i] == '*'){
					op = 444;
				}
                          	else if(expr[i] == '%'){
					op = 445;
				}
				if(flag == 2){ // 이미 operator가 나온 경우
					printf("too many operators\n");
					flag = 1;
					break;
				}
				if(buf[0] == '\0'){ // 첫 번째 피연산자가 없는 경우
					printf("missing first operand\n");
					flag = 1;
					break;
				}
				a = atoi(buf); // 첫번째 수
				memset(buf, '\0', sizeof(buf));
				idx = 0;
				flag = 2;
			}
			else if(isdigit(expr[i])){ // 숫자 생성
				buf[idx] = expr[i];
				idx++;
			}
			else{
				printf("unexpected expression\n");
				flag = 1;
				break;
			}
		}
		if(flag == 1) continue;
		if(op == -1){ // 연산자 없는 경우
			printf("missing operator\n");
			continue;
		}
		if(buf[0] == '\0'){ // 두 번째 피연산자 없는 경우
			printf("missing second operand\n");
			continue;
		}
		b = atoi(buf); // 두번째 수
		if(op==445 && b==0){ // divide by zero
			printf("cannot divide by zero\n");
			continue;
		}	
		long ret = syscall(op, a, b, &ans); // 미리 등록한 번호에 따라 system call 호출
		if(ret == -1){
			printf("error occurred while copying block data\n");
			exit(-1);
		}
		printf("ans = %d\n", ans);
	}
	return 0;
}
