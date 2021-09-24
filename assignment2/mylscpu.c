#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/utsname.h>

typedef struct vulnerability{
    char name[64];
    char detail[128];
} vuln;

typedef struct cpuinfo{
    char arch[32];      // Architecture
    char addr[64];      // Address sizes
    int cores;          // CPU(s)
    char online[8];     // On-line CPU(s) list
    int threads;        // Thread(s) per core
    char vender[32];    // Vender ID
    char family[8];     // CPU family
    char model[8];      // Model
    char name[64];      // Model name
    char stepping[8];   // Stepping
    char clock[16];     // CPU MHz
    char bogo[16];      // BogoMIPS
    char L1d[8];        // L1d cache
    char L1i[8];        // L1i cache
    char L2[8];         // L2 cache
    char L3[8];         // L3 cache
    int vulncnt;        // Vulnerablity count
    vuln vlist[16];     // Vulnerabilities
    char flags[1024];   // Flags
} CPU;

CPU *mycpu;
void printInfo();   // CPU 정보 출력하는 함수
void getArchInfo(); // architecture 정보 수집하는 함수
void getProcInfo(); // procfs 정보 수집하는 함수
void getSysInfo();  // sysfs 정보 수집하는 함수
int cmp(const void *, const void *); // 퀵소트 정렬 함수

int main(){ 
    mycpu = (CPU *)malloc(sizeof(CPU));
    getArchInfo();
    getProcInfo();
    getSysInfo();
    printInfo();
    return 0;
}

void printInfo(){
    printf("%-32s %s\n", "Architecture:", mycpu->arch);
    printf("%-32s %s\n", "Address sizes:", mycpu->addr);
    printf("%-32s %d\n", "CPU(s):", mycpu->cores);
    printf("%-32s %s\n", "On-line CPU(s) list:", mycpu->online);
    printf("%-32s %d\n", "Thread(s) per core:", mycpu->threads);
    printf("%-32s %s\n", "Vender ID:", mycpu->vender);
    printf("%-32s %s\n", "CPU family:", mycpu->family);
    printf("%-32s %s\n", "Model:", mycpu->model);
    printf("%-32s %s\n", "Model name:", mycpu->name);
    printf("%-32s %s\n", "Stepping:", mycpu->stepping);
    printf("%-32s %s\n", "CPU MHz:", mycpu->clock);
    printf("%-32s %s\n", "BogoMIPS:", mycpu->bogo);
    printf("%-32s %s\n", "L1d cache:", mycpu->L1d);
    printf("%-32s %s\n", "L1i cache:", mycpu->L1i);
    printf("%-32s %s\n", "L2 cache:", mycpu->L2);
    printf("%-32s %s\n", "L3 cache:", mycpu->L3);
    for(int i=0; i<mycpu->vulncnt; ++i){
        printf("Vulnerability ");
        printf("%-18s %s\n", mycpu->vlist[i].name, mycpu->vlist[i].detail);
    }
    printf("%-32s %s\n", "Flags:", mycpu->flags);
}

void getArchInfo(){
    struct utsname u;
    struct utsname *uptr = &u; // uname 리턴값을 받을 utsname 구조체 포인터
    uname(uptr);
    strcpy(mycpu->arch, u.machine);
}

void getProcInfo(){
    FILE *fp = fopen("/proc/cpuinfo", "r");
    // 가져올 정보 마킹
    int info[] = {0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0};
    int cores = 0, idx = 0;
    char buf[256];
    while(fgets(buf, sizeof(buf), fp)){
        if(strlen(buf) < 2){
            cores++; // cpuinfo 문단 개수
            continue;
        }
        if(idx > 25) continue; // 한개 코어 정보만 확인
        char *ptr = strtok(buf, ":"); // ':' 기준으로 문자열 자르기
        while (ptr != NULL){
            if(info[idx] == 1){ // 필요한 정보만 순서에 맞춰 구조체에 저장
                switch(idx){
                    case 1: strcpy(mycpu->vender, ptr+1); break; // space 한 칸 건너뜀
                    case 2: strcpy(mycpu->family, ptr+1); break;
                    case 3: strcpy(mycpu->model, ptr+1); break;
                    case 4: strcpy(mycpu->name, ptr+1); break;
                    case 5: strcpy(mycpu->stepping, ptr+1); break;
                    case 6: strcpy(mycpu->clock, ptr+1); break;
                    case 9: mycpu->threads = atoi(ptr+1); break; // sibling값
                    case 18: strcpy(mycpu->flags, ptr+1); break;
                    case 21: strcpy(mycpu->bogo, ptr+1); break;
                    case 24: strcpy(mycpu->addr, ptr+1); break; 
                }      
            }
            ptr = strtok(NULL, "\n"); // 다음 문자열 자르고 포인터 리턴    
        }
        idx++; // 다음 정보 확인
    }
    mycpu->cores = cores;
    mycpu->threads /= cores; // threads per core = sibling/cores
}

void getSysInfo(){
    // On-line CPU(s) list
    FILE *fp = fopen("/sys/devices/system/cpu/online", "r");
    char buf[256];
    fgets(buf, sizeof(buf), fp);
    strncpy(mycpu->online, buf, strlen(buf)-1); // 개행 문자 생략
    fclose(fp);

    // L1d, L1i, L2, L3 cache
    struct dirent *d; // dirent구조체
    char *dirname = "/sys/devices/system/cpu/cpu0/cache";
    DIR *dp = opendir(dirname);
    int cidx = 0; // cache idx
    while((d = readdir(dp)) != NULL){ // 디렉토리의 각 엔트리를 확인하며 index 서브디렉토리 개수를 count
        if(strstr(d->d_name, "index")){
            //printf("%s\n", d->d_name);
            cidx++; 
        }
    }
    for(int i=0; i<cidx; ++i){
        char idxpath[128], tmp[32], *unit = "KiB";

        // 캐시 사이즈 파싱
        sprintf(tmp, "%s%d%s", "/index", i, "/size");
        strcpy(idxpath, dirname);
        strcat(idxpath, tmp);
        fp = fopen(idxpath, "r");
        fgets(buf, sizeof(buf), fp);
        int csize = atoi(strtok(buf, "K")); // 프로세서당 cache 용량 추출
        fclose(fp);

        // 캐시 공유하는 logical processor 확인하고 사이즈 저장
        sprintf(tmp, "%s%d%s", "/index", i, "/shared_cpu_list");
        strcpy(idxpath, dirname);
        strcat(idxpath, tmp);

        // strcpy(idxpath, "test_shared_cpu_list"); // test file
        
        fp = fopen(idxpath, "r");
        fgets(buf, sizeof(buf), fp);
        int shcores_range[2], shc_len = strlen(buf)-1, num = 0;
        csize *= mycpu->cores; // 현재 논리 프로세서만 사용하는 캐시이면 프로세서 개수를 곱해줌
        if(shc_len > 1){ // 여러 프로세스가 공유하는 캐시인 경우
            for(int i=0; i<shc_len; ++i){           
                if(!isdigit(buf[i])){ // 구분 문자 나오는 경우 값 저장하고 넘어감
                    shcores_range[0] = num; // 첫 프로세서 번호
                    num = 0;
                    continue;
                }
                num = num*10+buf[i]-'0';
            }
            shcores_range[1] = num; // 마지막 프로세서 번호

            // 공유하는 프로세서 개수만큼 나눠줌
            int shcores = shcores_range[1]-shcores_range[0]+1;
            csize /= shcores;
        }
        fclose(fp);

        // 1024KiB 넘어가면 MiB로 단위 변환
        if(csize >= 1024){ 
            csize /= 1024;
            unit = "MiB";
        }
        sprintf(tmp, "%d %s", csize, unit);
        switch(i){
            case 0: strcpy(mycpu->L1d, tmp); break;
            case 1: strcpy(mycpu->L1i, tmp); break;
            case 2: strcpy(mycpu->L2, tmp); break;
            case 3: strcpy(mycpu->L3, tmp); break;
        }
    }

    // Vulnerabilities
    struct dirent *dlist[16]; // dirent구조체 배열
    int didx = 0; // dir entry idx
    dirname = "/sys/devices/system/cpu/vulnerabilities";
    dp = opendir(dirname);
    while((dlist[didx] = readdir(dp)) != NULL){
        if(dlist[didx]->d_name[0] != '.'){ // 취약점 파일만 확인
            didx++;
        }
    }
    qsort(dlist, didx, sizeof(struct dirent *), cmp); // d_name기준 오름차순 정렬
    for(int i=0; i<didx; ++i){
        char vulnpath[128];
        strcpy(vulnpath, dirname); 
        strcat(vulnpath, "/");
        strcat(vulnpath, dlist[i]->d_name);
        fp = fopen(vulnpath, "r");
        fgets(buf, sizeof(buf), fp);
        strcpy(mycpu->vlist[i].name, dlist[i]->d_name); // 취약점 이름
        strcat(mycpu->vlist[i].name, ":");
        strncpy(mycpu->vlist[i].detail, buf, strlen(buf)-1); // 취약점 내용
    }
    mycpu->vulncnt = didx; // 취약점 개수
}

int cmp(const void *entry1, const void *entry2){
    struct dirent *d1 = *(struct dirent **)entry1;
    struct dirent *d2 = *(struct dirent **)entry2;
    return strcmp(d1->d_name, d2->d_name);
}