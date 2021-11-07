#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/syscall.h>

int a[250][250] = {0, };
int b[250][250] = {0, };

void init(); // 랜덤한 숫자로 구성된 행렬을 생성하는 함수
void test(); // 행렬곱 연산을 수행하는 테스트 함수
void exit_func(); // 프로세스 종료시 정보를 출력하는 함수

int main(){
    pid_t pid;
    srand((unsigned)time(NULL));
    init();
    printf("----------- CFS with NICE -----------\n");
    for(int i=0; i<21; ++i){
        pid = fork();
        if(pid > 0) printf("%d process begins\n", pid);
        else if(pid == 0){
            // 1~7번째 생성 프로세스는 낮은 우선순위
            // 8~14번째 생성 프로세스까지 기본 우선순위
            // 15~21번째 생성 프로세스까지 높은 우선순위를 부여
            if(i < 7) setpriority(PRIO_PROCESS, getpid(), 19);
            else if(i < 14) setpriority(PRIO_PROCESS, getpid(), -20);
            atexit(exit_func); // 종료 루틴 등록
            test();
            exit(0);
        }
        else{
            fprintf(stderr, "fork() failed\n");
            exit(-1);
        }
    }
    while((pid = wait(NULL)) != -1); // 모든 자식 프로세스의 종료 대기
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

void exit_func(void){
    unsigned long long ret = syscall(442);
    printf("%d ends / NI: %3d / Burst: %llu\n", getpid(), getpriority(PRIO_PROCESS, getpid()), ret);
}