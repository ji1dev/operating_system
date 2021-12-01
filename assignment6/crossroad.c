#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#define MAX_CAR 15
#define NUM_OF_CROSS 4

bool BEGIN_NOW; // 현재 tick 기간에 출발한 차량 여부
int NUM_OF_CAR; // 전체 차량 수
int ELAPSED; // 총 소요 시간
int WAITING; // 대기중인 차량 수
int PASS_NOW; // 현재 tick 기간에 통과한 차량의 출발 지점
int SEQ[MAX_CAR]; // 각 시간별 차량 출발 지점 리스트
int READY[MAX_CAR]; // 출발 가능한 차량 리스트
int BEGIN[NUM_OF_CROSS]; // 각 차로별 진행 중 차량의 출발 시간
int PASSED[NUM_OF_CROSS+1]; // 각 차로별 통과 완료 차량 수, 전체 통과 완료 차량 수

pthread_t tid_arr[NUM_OF_CROSS]; // tid 번호 저장
sem_t sem; // 교차로 점유에 대한 semaphore

void init(); // 차량 수 입력 및 출발 지점 리스트를 생성하는 함수
void solve(); // 문제 해결 함수
void ready(int t); // 매 tick마다 차량을 출발 가능 상태로 설정하는 함수
void updateStat(int t); // 매 tick마다 교차로 상태를 업데이트하는 함수
void showStat(int t); // 현재 교차로 상태를 출력하는 함수
void showResult(int ); // 실행 결과를 출력하는 함수
void *run(void *arg); // 각 thread가 수행하는 함수

int main(){
    init();
    solve();
    return 0;
}

void init(){
    srand(time(NULL));
    sem_init(&sem, 0, 1); // semaphore를 wait중인 누군가가 바로 가져갈 수 있도록 초기화
    for(int i=0; i<MAX_CAR; ++i) READY[i] = -1; // 초기화
    for(int i=0; i<NUM_OF_CROSS; ++i) BEGIN[i] = -1;
    PASS_NOW = -1;

    printf("[*] Input num of vehicles : ");
    scanf("%d", &NUM_OF_CAR);
    if(NUM_OF_CAR<10 || NUM_OF_CAR>MAX_CAR){
        printf("[!] Out of bound (10~15)\n");
        exit(0);
    }

    for(int i=0; i<NUM_OF_CAR; ++i) SEQ[i] = rand()%4+1;

    printf("Total number of vehicles : %d\n", NUM_OF_CAR);
    printf("Start point : ");
    for(int i=0; i<NUM_OF_CAR; ++i){
        printf("%d ", SEQ[i]);
    }
    printf("\n");
}

void solve(){
    // 통과 완료 차량 수가 전체 차량 수보다 작으면 계속 수행
    while(PASSED[NUM_OF_CROSS] < NUM_OF_CAR){
        ELAPSED++; // 1초부터 매 초 차량 한대씩 출발 가능
        ready(ELAPSED-1); // 순서에 맞는 차량 출발 가능 상태로 변경

        // 각 출발 지점의 동작을 처리하는 thread 생성
        int pnum[4];
        for(int i=0; i<NUM_OF_CROSS; ++i){
            pnum[i] = i;
            if(pthread_create(&tid_arr[i], NULL, run, (void *)&pnum[i]) != 0){
                fprintf(stderr, "[!] pthread_create failed\n");
                exit(0);
            }
        }

        // 모든 thread의 종료를 기다림
        for(int i=0; i<NUM_OF_CROSS; ++i) pthread_join(tid_arr[i], NULL);

        updateStat(ELAPSED); // 통과 차량 정보 업데이트
        showStat(ELAPSED);
        BEGIN_NOW = false; // 현재 시점에 출발한 차량 여부 초기화
        PASS_NOW = -1; // 현재 시점에 도착한 차량의 출발 지점 번호 초기화
    }

    // 대기 차량 없는지 확인 후 최종 결과 출력
    if(WAITING == 0){
        ELAPSED++;
        showStat(ELAPSED);
        showResult(ELAPSED);
    }
}

void ready(int t){
    if(ELAPSED > NUM_OF_CAR) return; // 모든 차량이 출발 가능해지면 더 이상 준비 X
    READY[WAITING] = SEQ[t]; // 매 tick 마다 차량 한대씩 출발 가능하도록 리스트에 넣음
    WAITING++;
}

void * run(void *arg){
    // 아래 조건에 걸리는 경우 다른 출발 지점 thread에 semaphore를 release시킴
    // 1. 현재 출발 지점에서 시작되는 차로에 이미 차량이 진행중인 경우
    // 2. 인접 차로에 차량이 진행중인 경우
    // 3. 현재 시점에 다른 지점에서 이미 차량이 출발한 경우
    sem_wait(&sem);
    int p = *(int *)arg; // 출발 지점 번호 (0, 1, 2, 3)
    int next, prev;
    next = (p+1)>3 ? 0 : p+1;
    prev = (p-1)<0 ? 3 : p-1;
    if(BEGIN[p] != -1 || BEGIN[next] != -1 || BEGIN[prev] != -1 || BEGIN_NOW){
        sem_post(&sem);
        return NULL;
    }

    // 진행 불가 조건이 아니면 대기 리스트에서 현재 출발 지점에 해당되는 차량 선택
    int idx = -1;
    for(int i=0; i<WAITING; ++i){
        if(READY[i] == p+1){
            BEGIN[p] = ELAPSED; // 현재 출발 지점에서 차량 출발한 시간 저장
            BEGIN_NOW = true; // 현재 시간에 출발 차량 있음을 알림
            
            // 대기 리스트 업데이트
            WAITING--;
            for(int j=i; j<WAITING; ++j) READY[j] = READY[j+1];
            READY[WAITING+1] = -1;
            // printf("P%d-> 차로 점유\n", p+1);
            break;
        }
    }
    sem_post(&sem); // release semaphore
}

void updateStat(int t){
    for(int i=0; i<NUM_OF_CROSS; ++i){
        // 해당 출발 지점에서 출발한 차량이 존재하고,
        // 시작tick-현재tick값이 0이 아니라면 이전 시점에 출발한 차량이라는 의미이므로 통과 처리
        if(BEGIN[i]!=-1 && BEGIN[i]-t!=0){
            PASSED[i]++; // 현재 차로의 통과 완료 차량 수 증가
            PASSED[NUM_OF_CROSS]++; // 전체 통과 완료 차량 수 증가
            PASS_NOW = i+1; // 도착한 차량의 출발 지점 저장
            BEGIN[i] = -1; // 도착한 차량이 점유하던 차로를 비워줌
            break;
        }
    }
}

void showStat(int t){
    printf("tick : %d", t);
    printf("\n===============================");
    printf("\nPassed Vehicle");
    printf("\nCar ");
    // 현재 시점에 통과 차량 있는 경우에만 출력
    if(PASS_NOW != -1) printf("%d ", PASS_NOW);
    printf("\nWaiting Vehicle");
    printf("\nCar ");
    for(int i=0; i<WAITING; ++i){
        printf("%d ", READY[i]);
    }
    printf("\n===============================\n");
}

void showResult(int t){
    printf("Number of vehicles passed from each start point\n");
    for(int i=0; i<4; ++i){
        printf("P%d : %d times\n", i+1, PASSED[i]);
    }
    printf("Total time : %d ticks\n", t);
}