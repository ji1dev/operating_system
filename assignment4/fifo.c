#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/resource.h>

int a[250][250] = {0, };
int b[250][250] = {0, };

void init(); // 랜덤한 숫자로 구성된 행렬을 생성하는 함수
void test(); // 행렬곱 연산을 수행하는 테스트 함수

int main(){
    pid_t pid;
    srand((unsigned)time(NULL));
    init();
    printf("---------------- FIFO ----------------\n");
    for(int i=0; i<21; ++i){
        pid = fork();
        if(pid > 0) printf("%d process begins\n", pid);
        else if(pid == 0){
            test();
            exit(0);
        }
        else{
            fprintf(stderr, "fork() failed\n");
            exit(-1);
        }
    }
    // 모든 자식 프로세스의 종료 대기
    while((pid = wait(NULL)) != -1) printf("%d ends\n", pid);
    printf("--------- All processes ends ---------\n");
    return 0;
}

void init(){ 
    for(int i=0; i<250; ++i){
        for(int j=0; j<250; ++j){
            a[i][j] = rand()%10;
        }
    }
    for(int i=0; i<250; ++i){
        for(int j=0; j<250; ++j){
            b[i][j] = rand()%10;
        }
    }
}

void test(){
    for(int x=0; x<50; ++x){
        int sum = 0;
        for(int i=0; i<250; i++){
            for(int j=0; j<250; j++){
                sum = 0;
                for(int k=0; k<250; k++){
                    sum += a[i][k]*b[k][j];
                }
            }
        }
    }
}