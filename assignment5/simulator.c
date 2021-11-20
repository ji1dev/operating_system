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
bool isRef[MAX_FRAME]; // hit label

void input(); // 파일에서 frame개수와 prs를 읽어오는 함수
void init(); // 시뮬레이션에 필요한 값들을 초기화 하는 함수
void run(); // 페이지 교체 기법을 선택하는 함수
void simulate(int type); // 페이지 교체 시뮬레이션 함수
void saveResult(int time); // 매 time에 대한 시뮬레이션 결과를 저장하는 함수
void showResult(int type); // 시뮬레이션 결과를 출력하는 함수
int findVictim(int prs, int dir); // 현재 prs를 기준으로 victim을 찾는 함수

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
        fprintf(stderr, "File '%s' not exist\n", filename);
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
    for(int i=0; i<NUM_OF_FRAME; ++i){
        MEM[i] = 0; // page frame 초기화
        isRef[i] = false; // hit label 초기화
    }
    for(int i=0; i<PRS_CNT; ++i){
        isFault[i] = false; // page fault 기록 초기화
        for(int j=0; j<NUM_OF_FRAME; ++j){
            result[i][j] = 0; // 시뮬레이션 결과 초기화
        }
    }
}

void run(){
    int type;
    while(1){
        init();
        printf("\n1: OPT / 2: FIFO / 3: LRU / 4: Second-Chance / Others: exit\n");
        printf("Input simulation type : ");
        scanf("%d", &type);
        if(type>=1 && type<=4){
            simulate(type);
            showResult(type);
        }
        else exit(0);
    }
}

void simulate(int type){
    int idx = 0; // 현재 frame size
    int victim = 0; // victim page 번호
    for(int i=0; i<PRS_CNT; ++i){
        bool isHit = false;
        for(int j=0; j<idx; ++j){
            // page hit 발생한 경우
            if(MEM[j] == PRS[i]){
                isHit = true;
                // Second-Chance 방식이면 hit label 활성화
                if(type == 4) isRef[j] = true;
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
                if(type == 1){ // Optimal
                    victim = findVictim(i, 1); // i번째 prs 기준 OPT victim 선택
                    MEM[victim] = PRS[i];
                }
                else if(type == 2){ // FIFO
                    MEM[victim] = PRS[i];
                    victim = (victim+1)%NUM_OF_FRAME; // victim을 가장 오래전에 들어온 page로 유지
                }
                else if(type == 3){ // Least Recently Used
                    victim = findVictim(i, -1); // i번째 prs 기준 LRU victim 선택
                    MEM[victim] = PRS[i];
                }
                else if(type == 4){ // Second-Chance
                    // victim이 hit된 기록이 있으면 한번 더 기회를 주고 다음으로 넘어감
                    while(isRef[victim]){
                        isRef[victim] = false;
                        victim = (victim+1)%NUM_OF_FRAME;
                    }
                    MEM[victim] = PRS[i]; // hit label이 false인 victim을 찾으면 그 자리에 할당함
                    victim = (victim+1)%NUM_OF_FRAME; // victim을 가장 오래전에 들어온 page로 유지
                }
            }
        }
        saveResult(i); // i번째 PRS에 대한 할당 후 frame 상태를 저장
    }
}

void saveResult(int time){
    for(int i=0; i<NUM_OF_FRAME; ++i) result[time][i] = MEM[i];
}

void showResult(int type){
    char method[][16] = {"OPT", "FIFO", "LRU", "Second-Chance"};
    int faults = 0;
    // Input file informations
    printf("\n===========================================================\n");
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
        if(isFault[i]){ // page fault 여부 출력
            faults++;
            printf("F");
        }
        printf("\n");
    }
    printf("Number of page faults : %d times\n", faults);
    printf("===========================================================\n");
}

int findVictim(int prs, int dir){
    int v = 0, max_dist = -1;
    // 앞으로 가장 오랜시간 사용되지 않을 page를 victim으로 선택
    if(dir == 1){
        // 각 frame에 할당된 page들 중 prs에서 가장 멀리 있는 것을 찾음
        for(int i=0; i<NUM_OF_FRAME; ++i){
            int dist = 0;
            bool isFound = false;
            for(int j=prs+1; j<PRS_CNT; ++j){
                dist++;
                // 현재 frame에 할당된 page의 번호를 prs에서 찾은 경우,
                // 현재까지 가장 멀리있는 page보다 더 멀리있으면 prs 거리와 victim을 갱신함
                if(MEM[i] == PRS[j]){
                    isFound = true; // prs에서 일치하는 page 찾은 경우 flag 활성화
                    if(dist > max_dist){
                        max_dist = dist;
                        v = i;
                    }
                }
            }
            // isFound flag가 false이면 prs에서 찾기 못한 경우임
            // 즉, 앞으로 등장하지 않는 page이므로 victim으로 설정하고 탈출
            if(!isFound){
                v = i;
                break;
            }
        }
    }
    // 과거에 오랜시간 사용되지 않은 page를 victim으로 선택
    else if(dir == -1){
        for(int i=0; i<NUM_OF_FRAME; ++i){
            int dist = 0;
            bool isFound = false;
            for(int j=prs-1; j>=0; --j){
                dist++;
                // 각 frame에 할당된 page에 대해 가장 최근에 사용된 시점을 찾는 것이므로
                // prs에서 이미 page를 찾은 경우 더 이상 확인하지 않음
                if(!isFound && (MEM[i]==PRS[j])){
                    isFound = true;
                    if(dist > max_dist){
                        max_dist = dist;
                        v = i;
                        break;
                    }
                }
            }
        }
    }
    // printf("victim %d(page: %d)\n", v, MEM[v]);
    return v;
}