#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#define MAX_PRS 30
#define MAX_FRAME 4

int NUM_OF_FRAME; // frame 개수
int PRS_CNT; // page reference string 개수
int PRS[MAX_PRS]; // page reference string
int MEM[MAX_FRAME]; // page frames
int result[MAX_PRS][MAX_FRAME]; // 시뮬레이션 결과 저장
bool isFault[MAX_PRS]; // page fault 여부를 저장

void input(); // 파일에서 frame개수와 prs를 읽어오는 함수
void init(); // 시뮬레이션에 필요한 값들을 초기화 하는 함수
void run(); // 페이지 교체 기법을 선택하는 함수
void FIFO(); // FIFO 시뮬레이션 함수
void saveResult(int time); // 매 time에 대한 시뮬레이션 결과를 저장하는 함수
void showResult(int type); // 시뮬레이션 결과를 출력하는 함수

void FIFO(){
    int idx = 0; // 현재 frame size
    int victim = 0; // victim page 번호
    for(int i=0; i<PRS_CNT; ++i){
        bool isHit = false;
        for(int j=0; j<idx; ++j){
            // page hit 발생한 경우
            if(MEM[j] == PRS[i]){
                isHit = true;
            }
        }
        // page fault 발생한 경우
        if(!isHit){
            isFault[i] = true; // page fault 발생 기록
            // 사용된 frame 개수가 전체 개수보다 작은 경우 바로 할당
            if(idx < NUM_OF_FRAME){
                MEM[idx] = PRS[i];
                idx++;
            }
            // 가용 frame이 없으면 victim을 쫓아내고 그 자리에 할당
            else{
                MEM[victim] = PRS[i];
                victim = (victim+1)%NUM_OF_FRAME; // victim은 항상 가장 오래된 page
            }
        }
        saveResult(i); // i번째 PRS에 대한 할당 결과를 저장
    }
}

int main(){
    input();
    run();
    return 0;
}

void input(){
    FILE *fp;
    char filename[32];
    printf("Input file name : ");
    scanf("%s", filename);
    if((fp = fopen(filename, "r")) == NULL){
        fprintf(stderr, "%s not exist\n", filename);
        exit(0);
    }
    fscanf(fp, "%d", &NUM_OF_FRAME);
    while(!feof(fp)){
        fscanf(fp, "%d", &PRS[PRS_CNT]);
        PRS_CNT++;
    }
    PRS_CNT--;
    fclose(fp);
}

void init(){
    for(int i=0; i<NUM_OF_FRAME; ++i) MEM[i] = 0; // page frame 초기화
    for(int i=0; i<PRS_CNT; ++i) isFault[i] = false; // page fault 기록 초기화
    for(int i=0; i<PRS_CNT; ++i){
        for(int j=0; j<NUM_OF_FRAME; ++j){
            result[i][j] = 0; // 시뮬레이션 결과 초기화
        }
    }
}

void run(){
    int type;
    while(1){
        init();
        printf("\n1: OPT / 2: FIFO / 3: LRU / 4: Second-Chance / 5. exit\n");
        printf("Input simulation type : ");
        scanf("%d", &type);
        switch(type){
            case 1:
                showResult(type);
                break;
            case 2:
                FIFO();
                showResult(type);
                break;
            case 3:
                showResult(type);
                break;
            case 4:
                showResult(type);
                break;
            case 5:
                exit(0);
            default: break;
        }
    }
}

void saveResult(int time){
    for(int i=0; i<NUM_OF_FRAME; ++i){
        result[time][i] = MEM[i];
    }
}

void showResult(int type){
    char method[][16] = {"OPT", "FIFO", "LRU", "Second-Chance"};
    // Input file informations
    printf("\n==========================================================\n");
    printf("Used method : %s\n", method[type-1]);
    printf("page reference string : ");
    for(int i=0; i<PRS_CNT; ++i){
        printf("%d ", PRS[i]);
    }
    // column and row titles
    printf("\n\n%13s\t", "frame");
    for(int i=1; i<=NUM_OF_FRAME; ++i){
        printf("%-8d", i);
    }
    printf("%s\n", "page fault");
    printf("%s\n", "time");
    // simulation results
    for(int i=0; i<PRS_CNT; ++i){
        printf("%-13d\t", i+1);
        for(int j=0; j<NUM_OF_FRAME; ++j){
            if(result[i][j] != 0) printf("%-8d", result[i][j]);
            else printf("%-8s", " ");
        }
        if(isFault[i]) printf("F");
        printf("\n");
    }
    printf("==========================================================\n");
}