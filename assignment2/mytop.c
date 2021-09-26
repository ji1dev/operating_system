#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <math.h>
#include <time.h>
#include <utmp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/kdev_t.h>

#define _BSD_SOURCE
#define _GNU_SOURCE
#define MAX_PROC 8192
#define BUF_SIZE 1024

typedef struct procinfo{
    pid_t pid;                      // 프로세스 ID
    uid_t uid;                      // UID
    char username[16];              // 사용자명
    double cpu_usage;               // cpu점유율
    double mem_usage;               // 메모리 점유율
    unsigned long vsz;              // Virtual Memory Size (KiB)
    unsigned long rss;              // Resident Memory Size (KiB)
    unsigned long shm;              // Shared Memory Size (KiB)
    char tty[16];                   // 터미널 번호
    char state[8];                  // 상태
    char priority[4];               // 우선순위 값
    int nice;                       // nice 값
    char time[16];                  // 총 CPU사용시간
    char exename[512];              // 실행 파일
    char cmdline[1024];             // 명령어 (옵션 하나라도 붙는 경우)
    unsigned long utime;            // time spent in user mode in clock ticks
    unsigned long stime;            // time spent in kernel mode in clock ticks
    unsigned long st_time;          // time when the process started in clock ticks
    unsigned long long total_time;  // total time spent for process in seconds
} proc;

pid_t cur_pid;                      // 현재 pid
uid_t cur_uid;                      // 현재 uid
char cur_tty[16];                   // 현재 tty
unsigned long total_mem;            // total physical memory
unsigned long clk_tck;              // num of clock ticks per second of system
unsigned long uptime;               // uptime of system in seconds
time_t cur_time, prev_time;         // 현재 시간, 지난 리프레시 시간

proc plist[MAX_PROC];
int num_of_proc;

void init(); // 프로세스 정보 가져오기 전에 필요하거나 미리 설정 가능한 값을 가져오는 함수
void make_proclist_entry(); // 프로세스의 정보를 파싱 및 가공하는 함수
void clear_proclist_entry(); // 프로세스 정보를 초기화하는 함수
void clear_proclist(); // 프로세스 리스트를 초기화하는 함수
void print_summary(); // 최상단에 시스템 정보를 출력하는 함수
void print_proclist(); // 완성된 프로세스 정보 리스트를 출력하는 함수

int get_tty_nr(pid_t pid); // 프로세스의 컨트롤 터미널을 가져오는 함수
void get_tty(int tty_nr, char tty[16]); // 터미널 정보를 가져오는 함수
void get_total_mem(); // 물리 메모리 용량을 가져오는 함수
void get_uptime(); // 시스템의 uptime을 가져오는 함수
void get_username(); // username 가져오는 함수
void calc_cpu_usage(char *stat_path, proc *proc_entry); // cpu usage를 계산하는 함수
void get_msize(char *pid_path, proc *proc_entry); // vsz, rss, shm값을 가져오는 함수
void calc_mem_usage(unsigned long rss, double *ret); // memory usage를 계산하는 함수
void get_state(char *pid_path, pid_t pid, char state[8]); // state를 가져오는 함수
void get_priority(char *stat_path, proc *proc_entry); // priority, nice를 가져오는 함수
void calc_time_use(proc *proc_entry); // CPU 사용시간 계산하는 함수
void get_command(char *pid_path, proc *proc_entry); // 실행 명령어를 가져오는 함수

unsigned long convert_to_kb(unsigned long kib); // kib -> kb 단위 변환 함수

int main(){
    init();

    cur_time = time(NULL);
    
    make_proclist_entry();
    print_summary();
    print_proclist();
    clear_proclist();

    return 0;
}

void init(){
    cur_uid = getuid(); // current user id
    cur_pid = getpid(); // current pid
    clk_tck = sysconf(_SC_CLK_TCK); // clock ticks
    get_tty(get_tty_nr(cur_pid), cur_tty); // get current tty with tty_nr
    get_total_mem(); // total memory
}

void make_proclist_entry(){
    get_uptime();
    struct dirent *d;
    struct stat stat_buf; // 각 파일의 정보 구조체
    char *dirname = "/proc/";
    DIR *dp = opendir(dirname); // open proc dir
    while((d = readdir(dp)) != NULL){ // 디렉토리의 각 엔트리를 확인
        char pid_path[64];
        bool isProc = true;
        int name_len = strlen(d->d_name);
        pid_t p_pid;
        for(int i=0; i<name_len; ++i){
            if(!isdigit(d->d_name[i])){
                isProc = false;
                break;
            }
        }
        if(!isProc) continue; // 프로세스 파일이 아니면 다음 엔트리로 넘어감
        strcpy(pid_path, dirname);
        strcat(pid_path, d->d_name); // 프로세스 파일의 절대 경로
        sscanf(d->d_name, "%u", &p_pid); // pid 정수로 추출

        struct stat stat_buf;
        stat(pid_path, &stat_buf); // <pid>파일의 stat 구조체 가져옴
        if(!S_ISDIR(stat_buf.st_mode)) continue; // 파일이 directory가 아니면 넘어감

        proc proc_entry; // 프로세스 리스트 엔트리 생성
        clear_proclist_entry(&proc_entry); // 구조체 멤버 초기화
        
        // pid, uid 저장
        proc_entry.pid = p_pid;
        proc_entry.uid = stat_buf.st_uid;
        
        char stat_path[64];
        strcpy(stat_path, pid_path);
        strcat(stat_path, "/stat");
        FILE *fp = fopen(stat_path, "r"); // open stat file

        // username 저장
        get_username(proc_entry.uid, proc_entry.username);

        // cpu_usage 저장
        calc_cpu_usage(stat_path, &proc_entry);

        // vsz, rss, shm 저장
        get_msize(pid_path, &proc_entry);

        // mem_usage 저장
        calc_mem_usage(proc_entry.rss, &proc_entry.mem_usage);

        // tty 저장
        get_tty(get_tty_nr(proc_entry.pid), proc_entry.tty);

        // state 저장
        get_state(pid_path, proc_entry.pid, proc_entry.state);

        // priority, nice 저장
        get_priority(stat_path, &proc_entry);
        
        // time 저장
        calc_time_use(&proc_entry);

        // command 저장
        get_command(pid_path, &proc_entry);

        memcpy(&plist[num_of_proc], &proc_entry, sizeof(proc));
        num_of_proc++;
    }
    closedir(dp);
}

int get_tty_nr(pid_t pid){
    // tty_nr: controlling terminal of the process -> /proc/[pid]/stat의 7번째 토큰
    char path[64], buf[BUF_SIZE];
    sprintf(path, "%s%d%s", "/proc/", (int)pid, "/stat"); // stat 파일 경로 완성
    FILE *fp = fopen(path, "r"); // open stat file
    fgets(buf, BUF_SIZE, fp);
    char *ptr = strtok(buf, " ");
    int cnt = 1;
    while(cnt++ < 7) ptr = strtok(NULL, " "); // 7번째 토큰 추출
    fclose(fp);
    return atoi(ptr);
}

void get_tty(int tty_nr, char tty[16]){
    // pts: pseudo tty slave (major num == 136)
    // tty: teletype writer (major num == 4)
    //      -> (minor num < 64): console terminal
    //      -> (minor num >= 64): serial port
    int major_n, minor_n;
    major_n = MAJOR(tty_nr); // 디바이스 구분 위해 사용하는 주 디바이스 번호
	minor_n = MINOR(tty_nr); // 동일 디바이스 여러개일때 구분 위해 사용하는 부 디바이스 번호    
	if(major_n == 4){
		if(minor_n < 64) sprintf(tty, "%s%d", "tty", minor_n); // 콘솔 터미널 환경 e.g) tty1
		else sprintf(tty, "%sS%d", "tty", minor_n-64); // serial port(line)통한 통신 e.g)ttyS1
	}
	else if(major_n == 136){
		sprintf(tty, "%s/%d", "pts", minor_n); // 원격 터미널 환경 e.g) pts/1
	}
	else{
        sprintf(tty, "%s", "?");
    }
}

void get_total_mem(){
    FILE *fp = fopen("/proc/meminfo", "r"); // open meminfo file
    unsigned long tmp;
    char buf[BUF_SIZE];
    fgets(buf, BUF_SIZE, fp);
    char *ptr = buf;
    while(!isdigit(*ptr)) ptr++; // 값 나올때까지 ptr이동
    sscanf(ptr, "%lu", &tmp); // 숫자만 추출
    total_mem = convert_to_kb(tmp); // KiB단위를 kB단위로 변환
    fclose(fp);
}

void get_uptime(){
    FILE *fp = fopen("/proc/uptime", "r"); // open uptime file
    unsigned long tmp;
    char buf[BUF_SIZE];
    fgets(buf, BUF_SIZE, fp);
    char *ptr = strtok(buf, " "); // 첫 값을 가져옴
    sscanf(ptr, "%lu", &tmp);
    uptime = tmp;
    fclose(fp);
}

void get_username(uid_t uid, char user[32]){
    char tmp[32];
    struct passwd *pw = getpwuid(uid);
    strcpy(tmp, pw->pw_name); // passwd 파일에서 username 추출
    int user_len = strlen(tmp);
    if(user_len > 8) tmp[7] = '+'; // username이 8자리 이상이면 '+'기호로 ellipsis
    strncpy(user, tmp, 8); // 8자리까지 복사
}

void calc_cpu_usage(char *stat_path, proc *proc_entry){
    double elapsed_time; // total elapsed time since process started in seconds
    double usage;

    char buf[BUF_SIZE];
    FILE *fp = fopen(stat_path, "r"); // open stat file
    fgets(buf, BUF_SIZE, fp);

    char *ptr = strtok(buf, " ");
    int cnt = 0;
    while(cnt++ < 22){ // stat 파일에서 14, 15, 22번째 토큰 추출
        switch(cnt){
            case 14:
                sscanf(ptr, "%lu", &proc_entry->utime);
                break;
            case 15:
                sscanf(ptr, "%lu", &proc_entry->stime);
                break;
            case 22:
                sscanf(ptr, "%lu", &proc_entry->st_time);
                break;
        }
        ptr = strtok(NULL, " ");
    }
    proc_entry->total_time = proc_entry->utime + proc_entry->stime;
    elapsed_time = (double)(uptime-(proc_entry->st_time/clk_tck));
    usage = ((proc_entry->total_time/clk_tck)/elapsed_time)*100;
    if(usage<0 || usage>100 || isnan(usage) || isinf(usage)) usage = 0; // 표현할 수 없는 값 예외처리
    proc_entry->cpu_usage = usage;
    fclose(fp);
}

void get_msize(char *pid_path, proc *proc_entry){
    // SHR값 파싱
    char buf[BUF_SIZE], tmp_path[64];
    unsigned long rssfile = 0;
    strcpy(tmp_path, pid_path);
    strcat(tmp_path, "/status");
    FILE *fp = fopen(tmp_path, "r"); // open status file to get RssFile value
    int cnt = 0;
    while(cnt < 24){ // status 파일의 24번째 라인 추출
        fgets(buf, BUF_SIZE, fp);
        cnt++;
    }
    if(strstr(buf, "RssFile")){ // RssFile 항목이 있는 프로세스는 값 갱신
        char *ptr = strtok(buf, " ");
        cnt = 0;
        while(cnt++ < 1) ptr = strtok(NULL, " ");
        proc_entry->shm = atoi(ptr);
    }
    fclose(fp);

    // VSZ, RSS값 파싱
    memset(buf, BUF_SIZE, '\0');
    strcpy(tmp_path, pid_path);
    strcat(tmp_path, "/stat"); 
    fp = fopen(tmp_path, "r"); // open status file
    fgets(buf, BUF_SIZE, fp);
    char *ptr = strtok(buf, " ");
    cnt = 0;
    while(cnt++ < 24){ // stat 파일에서 23, 24번째 토큰 추출
        switch(cnt){
            case 23:
                sscanf(ptr, "%lu", &proc_entry->vsz); // KiB
                break;
            case 24:
                sscanf(ptr, "%lu", &proc_entry->rss); // kB
                break;
        }
        ptr = strtok(NULL, " ");
    }
    proc_entry->vsz = (proc_entry->vsz)/1024; // Byte -> KiB
    proc_entry->rss = (proc_entry->rss)*4; // (num of working segment + num of code segment pages)*4
    fclose(fp);
}

void calc_mem_usage(unsigned long rss, double *ret){
    // Calculate RSS value, divided by the size of the real memory in use, 
    // in the machine in KB, times 100, rounded to the nearest full percentage point.
    // Further, the rounding to the nearest percentage point. 
    //  -> 출력시 소수점 둘째 자리에서 반올림 후 첫째 자리까지 출력
    *ret = (double)rss/total_mem*100;
}

void get_state(char *pid_path, pid_t pid, char state[8]){
    char buf[BUF_SIZE], tmp_path[64];
    strcpy(tmp_path, pid_path);
    strcat(tmp_path, "/stat");
    FILE *fp = fopen(tmp_path, "r"); // open stat file
    fgets(buf, BUF_SIZE, fp);
    char *ptr = strtok(buf, " ");
    int cnt = 0;
    while(cnt++ < 2) ptr = strtok(NULL, " ");
    strcpy(state, ptr);
    fclose(fp);
}

void get_priority(char *stat_path, proc *proc_entry){
    char buf[BUF_SIZE];
    FILE *fp = fopen(stat_path, "r"); // open stat file
    fgets(buf, BUF_SIZE, fp);
    char *ptr = strtok(buf, " ");
    int cnt = 0;
    while(cnt++ < 19){ // stat 파일에서 18, 19번째 토큰
        switch(cnt){
            case 18:
                sscanf(ptr, "%s", proc_entry->priority);
                // PR값 -100이면 최고 우선순위인 realtime process -> "rt"로 표기
                if(!strcmp(proc_entry->priority, "-100")){
                    strcpy(proc_entry->priority, "rt");
                }
                break;
            case 19:
                sscanf(ptr, "%d", &proc_entry->nice);
                break;
        }
        ptr = strtok(NULL, " ");
    }
    fclose(fp);
}

void calc_time_use(proc *proc_entry){
    // Reflecting more granularity through hundredths of a second (centisecond)
    // (utime+ctime)*100/clk_tck = total time in centisec
    unsigned long tt_time = proc_entry->total_time/clk_tck;
    int csec_part = proc_entry->total_time*100/clk_tck%100; 
	struct tm *tms = gmtime(&tt_time); // GMT+9 보정 안되도록 gmtime사용

    // 999분 넘어가면 centisec 필드 제외
    if(tms->tm_min > 999) sprintf(proc_entry->time, "%d:%02d", tms->tm_min, tms->tm_sec);
    else sprintf(proc_entry->time, "%d:%02d.%02d", tms->tm_min, tms->tm_sec, csec_part); 
}

void get_command(char *pid_path, proc *proc_entry){
    // 실행파일명 파싱 
    char buf[BUF_SIZE], tmp_path[64];
    strcpy(tmp_path, pid_path);
    strcat(tmp_path, "/stat");
    FILE *fp = fopen(tmp_path, "r"); // open stat file
    fgets(buf, BUF_SIZE, fp);
    int cnt = 0;
    char *ptr = strtok(buf, "(");
    while(cnt++ < 1) ptr = strtok(NULL, ")"); // stat 파일에서 2번째 토큰 추출
    strcpy(proc_entry->exename, ptr);
    fclose(fp);

    // 명령줄 인자 파싱
    strcpy(tmp_path, pid_path);
    strcat(tmp_path, "/cmdline");
    fp = fopen(tmp_path, "r"); // open cmdline file
   
    char *arg = 0;
    size_t size = 1;
    while(getdelim(&arg, &size, 0, fp) != -1){ // delimiter로 끊어서 읽음
        strcat(proc_entry->cmdline, arg);
        strcat(proc_entry->cmdline, " "); // 인자 구분을 위한 space 추가 
    }

    // 인자가 없는 경우 [exename]으로 저장
    if(strlen(proc_entry->cmdline) == 0){
        sprintf(proc_entry->cmdline, "[%s]", proc_entry->exename);
    }
}

void clear_proclist_entry(proc *proc_entry){
    memset(proc_entry->username, '\0', 16);
    proc_entry->uid = 0;
    proc_entry->pid = 0;
    proc_entry->cpu_usage = 0;
    proc_entry->mem_usage = 0;
    proc_entry->vsz = 0;
    proc_entry->rss = 0;
    proc_entry->shm = 0;
    memset(proc_entry->tty, '\0', 16);
    memset(proc_entry->state, '\0', 8);
    memset(proc_entry->state, '\0', 4);
    proc_entry->nice = 0;
    memset(proc_entry->time, '\0', 16);
    memset(proc_entry->exename, '\0', 1024);
    memset(proc_entry->cmdline, '\0', 1024);
    proc_entry->utime = 0;
    proc_entry->stime = 0;
    proc_entry->st_time = 0;
    proc_entry->total_time = 0;
}

void clear_proclist(){
    for(int i=0; i<num_of_proc; ++i){
        clear_proclist_entry(&plist[i]);
    }
    num_of_proc = 0;
}

/*
top - 00:09:09 up 1 day, 11:45,  3 users,  load average: 0.01, 0.06, 0.08
Tasks: 210 total,   1 running, 209 sleeping,   0 stopped,   0 zombie
%Cpu(s):  0.5 us,  0.7 sy,  0.0 ni, 98.7 id,  0.0 wa,  0.0 hi,  0.2 si,  0.0 st
MiB Mem :   3932.8 total,    794.3 free,    863.2 used,   2275.3 buff/cache
MiB Swap:   2048.0 total,   2048.0 free,      0.0 used.   2811.8 avail Mem 
*/
void print_summary(){
    // Current time
    char curtime_s[32];
    struct tm *tms= localtime(&cur_time);
    sprintf(curtime_s, "top - %02d:%02d:%02d ", tms->tm_hour, tms->tm_min, tms->tm_sec);
    printf("%s ", curtime_s);

    // Uptime
    char uptime_s[32];
    tms = gmtime(&uptime); // GMT+9 보정 안되도록 gmtime사용
    if(uptime < 60*60) sprintf(uptime_s, "up%2d min, ", tms->tm_min);
	else if(uptime < 60*60*24) sprintf(uptime_s, "up%2d:%02d, ", tms->tm_hour, tms->tm_min);
	else{
        if(tms->tm_yday > 1) sprintf(uptime_s, "up%2d day, %02d:%02d, ", tms->tm_yday, tms->tm_hour, tms->tm_min);
        else sprintf(uptime_s, "up%2d days, %02d:%02d, ", tms->tm_yday, tms->tm_hour, tms->tm_min);
    }
    printf("%s ", uptime_s);

    // Num of users
    int active_users = 0;
    char active_s[16];
    struct utmp *uts;
    setutent();
    while((uts = getutent()) != NULL){ // utent 읽어서 type이 USER이고, 값이 있으면 count++
        if((uts->ut_type == USER_PROCESS) && strlen(uts->ut_user)) active_users++;
    }
    if(active_users > 1) sprintf(active_s, "%d users, ", active_users);
    else sprintf(active_s, "%d user, ", active_users); 
    endutent();
    printf("%s ", active_s);

    // Load average
    char loadavg_s[32], buf[BUF_SIZE];
    strcpy(loadavg_s, "load average: ");
    FILE *fp = fopen("/proc/loadavg", "r");
    fgets(buf, BUF_SIZE, fp);
    char *ptr = strtok(buf, " ");
    int cnt = 0;
    while(cnt++ < 3){ // loadavg 파일에서 3번째 토큰까지 읽어옴
        strcat(loadavg_s, ptr);
        strcat(loadavg_s, " ");
        ptr = strtok(NULL, " ");
    }
    fclose(fp);
    printf("%s\n", loadavg_s);

    // Tasks (running, sleeping, stopped, zombie)

    // CPU states (user, sys, nice, idle, IO-wait, hw/sw interrupt, stolen by hyperviser)

    // Memory usage (Physical and Virtual memory)

}

void print_proclist(){
    // 터미널 창의 크기를 가져옴
    struct winsize term;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &term);
    int term_width = term.ws_col;

    char tmp[2048], result[2048];
    sprintf(tmp, "%7s %-8s %3s %3s %7s %7s %7s %s %5s %5s %9s %s"
            , "PID", "USER", "PR", "NI"
            , "VIRT", "RES", "SHR", "S"
            , "\%CPU", "%MEM", "TIME+", "COMMAND");

    strncpy(result, tmp, term_width-1);
    printf("%s\n", result);

    for(int i=0; i<num_of_proc; ++i){
        sprintf(tmp, "%7u %-8s %3s %3d %7lu %7lu %7lu %s %5.1lf %5.1lf %9s %s"
                , plist[i].pid, plist[i].username, plist[i].priority, plist[i].nice
                , plist[i].vsz, plist[i].rss, plist[i].shm, plist[i].state
                , plist[i].cpu_usage, plist[i].mem_usage, plist[i].time, plist[i].exename);
        strncpy(result, tmp, term_width-1);
        printf("%s\n", result);
    }
}

unsigned long convert_to_kb(unsigned long kib){
    return kib*1024/1000; // 1.024를 곱함
}