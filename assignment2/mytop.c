// gcc -o mytop mytop.c -lncurses
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
#include <ncurses.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/kdev_t.h>

#define _BSD_SOURCE
#define _GNU_SOURCE
#define MAX_PROC 8192
#define MAX_PID 32768
#define BUF_SIZE 1024
#define toMiB 1000/1.024

typedef struct procinfo{
    pid_t pid;                          // 프로세스 ID
    uid_t uid;                          // UID
    char username[16];                  // 사용자명
    long double cpu_usage;              // cpu점유율
    long double mem_usage;              // 메모리 점유율
    unsigned long vsz;                  // Virtual Memory Size (KiB)
    unsigned long rss;                  // Resident Memory Size (KiB)
    unsigned long shm;                  // Shared Memory Size (KiB)
    char tty[32];                       // 터미널 번호
    char state[8];                      // 상태
    char priority[8];                   // 우선순위 값
    int nice;                           // nice 값
    char time[16];                      // 총 CPU사용시간
    char exename[512];                  // 실행 파일
    char cmdline[1024];                 // 명령줄 인자
    unsigned long utime;                // time spent in user mode in clock ticks
    unsigned long stime;                // time spent in kernel mode in clock ticks
    unsigned long st_time;              // time when the process started in clock ticks
    unsigned long long total_time;      // total time spent for process
} proc;

pid_t cur_pid;                          // 현재 pid
uid_t cur_uid;                          // 현재 uid
char cur_tty[16];                       // 현재 tty
time_t cur_time, prev_time;             // 현재 시간, 지난 리프레시 시간
unsigned long total_mem;                // total physical memory
unsigned long clk_tck;                  // num of clock ticks per second of system
unsigned long uptime, prev_uptime;      // uptime of system in seconds
unsigned long prev_cpu_ticks[8];        // 이전 CPU tick (CPU states summary에서 사용)
unsigned long prev_cpu_time[MAX_PID];   // previous total time spent for process

proc plist[MAX_PROC];
int num_of_proc;
int row, col;                           // 출력 기준이 되는 row, col좌표
double refresh_delay = 3.0;             // 기본 refresh delay값
bool toggleCMD = false;                 // cmdname과 line 토글
int sort_option = 0;                   // 정렬 기준 (0: %CPU, 1: %MEM, 2: TIME+)

void init(); // 프로세스 정보 가져오기 전에 필요하거나 미리 설정 가능한 값을 가져오는 함수
void make_proclist_entry(); // 프로세스의 정보를 파싱 및 가공하는 함수
void sort_proclist(); // CPU 사용률 순으로 프로세스 리스트를 정렬하는 함수
void clear_proclist_entry(); // 프로세스 정보를 초기화하는 함수
void clear_proclist(); // 프로세스 리스트를 초기화하는 함수
void print_summary(); // 최상단에 시스템 정보를 출력하는 함수
void print_proclist(); // 완성된 프로세스 정보 리스트를 출력하는 함수

int get_tty_nr(pid_t pid); // 프로세스의 컨트롤 터미널을 가져오는 함수
void get_tty(int tty_nr, char tty[16]); // 터미널 정보를 가져오는 함수
void get_total_mem(); // 물리 메모리 용량을 가져오는 함수
void get_uptime(); // 시스템의 uptime을 가져오는 함수
void get_username(uid_t uid, char user[16]); // username 가져오는 함수
void calc_cpu_usage(char *stat_path, proc *proc_entry); // cpu usage를 계산하는 함수
void get_msize(char *pid_path, proc *proc_entry); // vsz, rss, shm값을 가져오는 함수
void calc_mem_usage(unsigned long rss, long double *ret); // memory usage를 계산하는 함수
void get_state(char *pid_path, pid_t pid, char state[8]); // state를 가져오는 함수
void get_priority(char *stat_path, proc *proc_entry); // priority, nice를 가져오는 함수
void calc_time_use(proc *proc_entry); // CPU 사용시간 계산하는 함수
void get_command(char *pid_path, proc *proc_entry); // 실행 명령어를 가져오는 함수

int cmp0(const void *p1, const void *p2);
int cmp1(const void *p1, const void *p2);
int cmp2(const void *p1, const void *p2);

int main(){
    initscr(); // curse모드를 시작
    keypad(stdscr, TRUE); // 방향키 등 특수키 입력을 허용
    noecho(); // 입력된 문자를 화면에 출력하지 않도록 함
    timeout(50); // read blocks for 50ms delay
    curs_set(0); // 커서 숨김

    cur_time = time(NULL);
    row = 0;
    col = 0;
    init();
    make_proclist_entry();
    sort_proclist();
    print_summary();
    print_proclist();
    refresh(); // 실제 화면에 출력

    while(1){
        int ch = getch();
        cur_time = time(NULL); // 현재시간 갱신
        bool isRefresh = false;
        char str[32];
        if(ch=='q') break;
        switch(ch){ // interactive commands
            case ' ': // refresh all
                isRefresh = true;
                break;
            case 'c': // toggle command name and line
                isRefresh = true;
                toggleCMD = toggleCMD ? false : true;
                break;
            case 'P': // cpu usage로 정렬
                isRefresh = true;
                sort_option = 0;
                break;
            case 'M': // mem usage로 정렬
                isRefresh = true;
                sort_option = 1;
                break;
            case 'T': // total time으로 정렬
                isRefresh = true;
                sort_option = 2;
                break;
            case KEY_UP:
                isRefresh = true;
                row--;
                if(row < 0) row = 0; // 터미널 바깥으로 벗어나면 0으로 고정
                break;
            case KEY_DOWN:
                isRefresh = true;
                row++;
                if(row > num_of_proc-1) row = num_of_proc-1; // 프로세스는 항상 1개는 표시
                break;
            case KEY_LEFT:
                isRefresh = true;
                col -= 8;
                if(col < 0) col = 0; // 터미널 바깥으로 벗어나면 0으로 고정
                break;
            case KEY_RIGHT:
                isRefresh = true;
                col += 8;
                break;
        }
        // refresh조건을 만족하거나 3초 지나면 수행
        if(isRefresh || cur_time-prev_time >= 3){
            clear(); // 화면 클리어
            clear_proclist();
            make_proclist_entry();
            sort_proclist();
            print_summary();
            print_proclist();
            refresh();
            prev_time = cur_time; // 현재시간 저장
        }
    }
    endwin(); // curses모드 종료
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

        // pid 디렉토리 여부 확인
        char pid_path[64];
        strcpy(pid_path, dirname);
        strcat(pid_path, d->d_name); // 프로세스 파일의 절대 경로
        sscanf(d->d_name, "%u", &p_pid); // pid 정수로 추출

        struct stat stat_buf;
        stat(pid_path, &stat_buf); // <pid>파일의 stat 구조체 가져옴
        if(!S_ISDIR(stat_buf.st_mode)) continue; // 파일이 directory가 아니면 넘어감

        if(access(pid_path, R_OK) < 0){
            fprintf(stderr, "%s\n", strerror(errno));
            continue;
        }

        // 파일 접근 권한 확인
        FILE *fp;
        char stat_path[64];
        strcpy(stat_path, pid_path);
        strcat(stat_path, "/stat");
        if(access(stat_path, R_OK) < 0){
            fprintf(stderr, "%s\n", strerror(errno));
            continue;
        }
        if((fp=fopen(stat_path, "r")) == NULL){ // open stat file
            fprintf(stderr, "%s\n", strerror(errno));
            continue;
        }

        // 프로세스 리스트 엔트리 생성 및 초기화
        proc proc_entry; 
        clear_proclist_entry(&proc_entry);
        
        // pid, uid 저장
        proc_entry.pid = p_pid;
        proc_entry.uid = stat_buf.st_uid;

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
        fclose(fp);
    }
    closedir(dp);
}

void sort_proclist(){
    if(sort_option == 0) qsort(plist, num_of_proc, sizeof(plist[0]), cmp0);
    else if(sort_option == 1) qsort(plist, num_of_proc, sizeof(plist[0]), cmp1);
    else qsort(plist, num_of_proc, sizeof(plist[0]), cmp2);
}

int get_tty_nr(pid_t pid){
    // tty_nr: controlling terminal of the process -> /proc/[pid]/stat의 7번째 토큰
    char path[64], buf[BUF_SIZE];
    sprintf(path, "%s%d%s", "/proc/", (int)pid, "/stat"); // stat 파일 경로 완성
    FILE *fp=fopen(path, "r"); // open stat filea
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
    total_mem = tmp;
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

void get_username(uid_t uid, char user[16]){
    char tmp[32];
    struct passwd *pw;
    if((pw = getpwuid(uid)) == NULL){
        fprintf(stderr, "%s\n", strerror(errno));
        exit(-1);
    }
    strcpy(tmp, pw->pw_name); // passwd 파일에서 username 추출
    int user_len = strlen(tmp);
    if(user_len > 8) tmp[7] = '+'; // username이 8자리 이상이면 '+'기호로 ellipsis
    strncpy(user, tmp, 8); // 8자리까지 복사
}

void calc_cpu_usage(char *stat_path, proc *proc_entry){
    long double elapsed_time; // total elapsed time since process started
    long double usage;
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
    // based on the interval since the last refresh
    // prev cpu total time 값이 있으면 리프레시 된 경우이므로 이전 값과의 차이 계산
    proc_entry->total_time = proc_entry->utime + proc_entry->stime;
    if(prev_cpu_time[proc_entry->pid] != 0){
        elapsed_time = (long double)(cur_time-prev_time);
        usage = (((proc_entry->total_time-prev_cpu_time[proc_entry->pid])/elapsed_time)/clk_tck)*100;
    }
    else{
        elapsed_time = (long double)(uptime-proc_entry->st_time);
        usage = ((proc_entry->total_time/clk_tck)/elapsed_time)*100;
    }
    if(usage<0 || usage>100 || isnan(usage) || isinf(usage)) usage = 0; // 표현할 수 없는 값 예외처리
    proc_entry->cpu_usage = usage;
    prev_cpu_time[proc_entry->pid] = proc_entry->total_time; // previous cpu total time
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

void calc_mem_usage(unsigned long rss, long double *ret){
    // Calculate RSS value, divided by the size of the real memory in use, 
    // in the machine in KB, times 100, rounded to the nearest full percentage point.
    // Further, the rounding to the nearest percentage point. 
    //  -> 출력시 소수점 둘째 자리에서 반올림 후 첫째 자리까지 출력
    *ret = (double)rss/total_mem*100;
}

void get_state(char *pid_path, pid_t pid, char state[8]){
    // RSDZTW: 기본 state -> stat 3번째 토큰
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
    fp = fopen(tmp_path, "r");
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
    fclose(fp);
}

void clear_proclist_entry(proc *proc_entry){
    proc_entry->pid = 0;
    proc_entry->uid = 0;
    memset(proc_entry->username, '\0', 16);
    proc_entry->cpu_usage = 0.0;
    proc_entry->mem_usage = 0.0;
    proc_entry->vsz = 0;
    proc_entry->rss = 0;
    proc_entry->shm = 0;
    memset(proc_entry->tty, '\0', 32);
    memset(proc_entry->state, '\0', 8);
    memset(proc_entry->priority, '\0', 8);
    proc_entry->nice = 0;
    memset(proc_entry->time, '\0', 16);
    memset(proc_entry->exename, '\0', 512);
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

void print_summary(){
    // Current time
    char curtime_s[32];
    struct tm *tms= localtime(&cur_time);
    sprintf(curtime_s, "top - %02d:%02d:%02d ", tms->tm_hour, tms->tm_min, tms->tm_sec);

    // Uptime
    char uptime_s[32];
    tms = gmtime(&uptime); // GMT+9 보정 안되도록 gmtime사용
    if(uptime < 60*60) sprintf(uptime_s, "up%2d min, ", tms->tm_min);
	else if(uptime < 60*60*24) sprintf(uptime_s, "up%2d:%02d, ", tms->tm_hour, tms->tm_min);
	else{
        if(tms->tm_yday > 1) sprintf(uptime_s, "up%2d day, %02d:%02d, ", tms->tm_yday, tms->tm_hour, tms->tm_min);
        else sprintf(uptime_s, "up%2d days, %02d:%02d, ", tms->tm_yday, tms->tm_hour, tms->tm_min);
    }

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
    mvprintw(0, 0, "%s %s %s %s", curtime_s, uptime_s, active_s, loadavg_s); // 0행 0열로 커서 이동 후 출력

    // Tasks (running, sleeping, stopped, zombie)
    char task_s[128];
	int running = 0, sleeping = 0, stopped = 0, zombie = 0;
	for(int i=0; i<num_of_proc; ++i){
		if(!strcmp(plist[i].state, "R")) running++;
		else if(!strcmp(plist[i].state, "S")) sleeping++;
		else if(!strcmp(plist[i].state, "I")) sleeping++;
		else if(!strcmp(plist[i].state, "T")) stopped++;
		else if(!strcmp(plist[i].state, "Z")) zombie++;
	}
    sprintf(task_s, "Tasks: %4d total, %4d running, %4d sleeping, %4d stopped, %4d zombie"
            ,num_of_proc, running, sleeping, stopped, zombie);
    mvprintw(1, 0, "%s", task_s);

    // CPU states (user, sys, nice, idle, IO-wait, hw/sw interrupt, stolen by hyperviser)
    // based on the interval since the last refresh
    double cpu_ticks[8], cpu_sec[8]; // us, ni, sy, id, wa, hi, si, st 순서대로
    char cpu_s[128];
    fp = fopen("/proc/stat", "r");
    fgets(buf, BUF_SIZE, fp);
    char *ptr_cpu = buf;
    while(!isdigit(*ptr_cpu)) ptr_cpu++; // 값 나올때까지 ptr이동
	sscanf(ptr_cpu, "%lf %lf %lf %lf %lf %lf %lf %lf"
            ,&cpu_ticks[0], &cpu_ticks[1], &cpu_ticks[2], &cpu_ticks[3]
            ,&cpu_ticks[4], &cpu_ticks[5], &cpu_ticks[6], &cpu_ticks[7]);

    unsigned long time_tick = 0, cur_uptime = 0;
    for(int i=0; i<8; ++i) cur_uptime += cpu_ticks[i]; // 헤더 뿌려주는 부분에서만 사용하는 uptime (/proc/stat값의 총합)

    if(prev_uptime == 0){ // 처음 값을 뿌려주는 경우
        time_tick = cur_uptime; // 시스템 uptime기준 tick으로 변환
        for(int i=0; i<8; ++i){
            cpu_sec[i] = cpu_ticks[i]; // 최초 저장하는 값은 파싱해온 값 그대로를 사용
        }
    }
    else{ // 프로그램 실행 중 리프레시 발생한 경우
        time_tick = (cur_uptime-prev_uptime); 
        for(int i=0; i<8; ++i){ // 누적값이므로 이전값만큼 빼고 계산함
            cpu_sec[i] = cpu_ticks[i]-prev_cpu_ticks[i];
        }
    }

    for(int i=0; i<8; ++i){
		cpu_sec[i] = (cpu_sec[i]/time_tick)*100;
        if(cpu_sec[i]<0 || cpu_sec[i]>100 || isnan(cpu_sec[i]) || isinf(cpu_sec[i])) cpu_sec[i] = 0; // 표현할 수 없는 값 예외처리	
	}

    sprintf(cpu_s, "%%Cpu(s): %4.1lf us, %4.1lf sy, %4.1lf ni, %4.1lf id, %4.1lf wa, %4.1lf hi, %4.1lf si, %4.1lf st"
            , cpu_sec[0], cpu_sec[2], cpu_sec[1], cpu_sec[3]
            , cpu_sec[4], cpu_sec[5], cpu_sec[6], cpu_sec[7]);
    mvprintw(2, 0, "%s", cpu_s);
    
    prev_uptime = cur_uptime; // 이전 uptime, cpu ticks값 저장
    for(int i=0; i<8; ++i) prev_cpu_ticks[i] = cpu_ticks[i];
    fclose(fp);

    // Memory usage (Physical and Virtual memory)
    char mem_s[2][1024];
    unsigned long memfree, memavail, buffers, cached, swaptotal, swapfree, srec, mem_used, swap_used, bufcache;
    memset(buf, '\0', BUF_SIZE);
    fp = fopen("/proc/meminfo", "r");
    cnt = 0;
    while(cnt++ < 24){
        char *ptr_m;
        fgets(buf, BUF_SIZE, fp);
        switch(cnt){
            case 2: // mem free
                ptr_m = buf;
                while(!isdigit(*ptr_m)) ptr_m++;
                sscanf(ptr_m, "%lu", &memfree);
                break;
            case 3: // mem available
                ptr_m = buf;
                while(!isdigit(*ptr_m)) ptr_m++;
                sscanf(ptr_m, "%lu", &memavail);
                break;
            case 4: // buffers
                ptr_m = buf;
                while(!isdigit(*ptr_m)) ptr_m++;
                sscanf(ptr_m, "%lu", &buffers);
                break;
            case 5: // cached
                ptr_m = buf;
                while(!isdigit(*ptr_m)) ptr_m++;
                sscanf(ptr_m, "%lu", &cached);
                break;
            case 15: // swap total
                ptr_m = buf;
                while(!isdigit(*ptr_m)) ptr_m++;
                sscanf(ptr_m, "%lu", &swaptotal);
                break;
            case 16: // swap free
                ptr_m = buf;
                while(!isdigit(*ptr_m)) ptr_m++;
                sscanf(ptr_m, "%lu", &swapfree);
                break;
            case 24: // sreclaimable
                ptr_m = buf;
                while(!isdigit(*ptr_m)) ptr_m++;
                sscanf(ptr_m, "%lu", &srec);
                break;  
        }     
    }
    mem_used = total_mem-memfree-buffers-cached-srec;
    swap_used = swaptotal-swapfree;
    bufcache = buffers+cached+srec;
    sprintf(mem_s[0], "MiB Mem : %8.1lf total, %8.1lf free, %8.1lf used, %8.1lf buff/cache\n"
            ,(double)total_mem/toMiB, (double)memfree/toMiB, (double)mem_used/toMiB, (double)bufcache/toMiB); // MiB단위로 변환해서 저장
    sprintf(mem_s[1], "MiB Swap: %8.1lf total, %8.1lf free, %8.1lf used, %8.1lf avail Mem\n"
            ,(double)swaptotal/toMiB, (double)swapfree/toMiB, (double)swap_used/toMiB, (double)memavail/toMiB);
    mvprintw(3, 0, "%s", mem_s[0]);
    mvprintw(4, 0, "%s", mem_s[1]);
    fclose(fp);
}

void print_proclist(){
    // 빈 라인 출력 (5행)
    for(int i=0; i<COLS; ++i) mvprintw(5, i, " ");

    // Title 출력 (6행)
    char tmp[2048], result[2048];
    attron(A_REVERSE); // 색 반전
    for(int i=0; i<COLS; ++i) mvprintw(6, i, " "); // 6행 전체를 빈 문자로 채움
    sprintf(tmp, "%7s %-8s %3s %3s %7s %7s %7s %s %5s %5s %9s %s"
            , "PID", "USER", "PR", "NI"
            , "VIRT", "RES", "SHR", "S"
            , "\%CPU", "%MEM", "TIME+", "COMMAND");
    strcpy(result, tmp);
    int offset = col; // offset초기값은 col시작 위치
    // COMMAND column title은 오른쪽으로 끝까지 이동해도 표시됨
    if(strlen(result)-9 < offset)  mvprintw(6, 0, "%s", "COMMAND");
    else mvprintw(6, 0, "%s", result+offset);
    attroff(A_REVERSE); // 색 반전 종료

    // Entry 출력 (7행부터)
    int cur_row = 7;
    for(int i=row; i<num_of_proc; ++i){
        if(cur_row > LINES) break; // 한 행씩 내려가며 출력, 높이 초과하면 출력 중지
        if(toggleCMD){ // cmd toggle true이면 cmdline 출력, 아니면 name 출력
            sprintf(tmp, "%7u %-8s %3s %3d %7lu %7lu %7lu %s %5.1Lf %5.1Lf %9s %s"
                , plist[i].pid, plist[i].username, plist[i].priority, plist[i].nice
                , plist[i].vsz, plist[i].rss, plist[i].shm, plist[i].state
                , plist[i].cpu_usage, plist[i].mem_usage, plist[i].time, plist[i].cmdline);
        }
        else{
            sprintf(tmp, "%7u %-8s %3s %3d %7lu %7lu %7lu %s %5.1Lf %5.1Lf %9s %s"
                , plist[i].pid, plist[i].username, plist[i].priority, plist[i].nice
                , plist[i].vsz, plist[i].rss, plist[i].shm, plist[i].state
                , plist[i].cpu_usage, plist[i].mem_usage, plist[i].time, plist[i].exename);
        }
        strcpy(result, tmp);
        offset = col;
        // 더 이상 문자열을 출력하면 안되는 col값에 도달하면 offset을 문자열 길이로 고정
        if(strlen(result) < offset) offset = strlen(result);
        if(!strcmp(plist[i].state, "R")) attron(A_BOLD); // running process는 bold효과로 출력
        mvprintw(cur_row++, 0, "%s", result+offset);
        attroff(A_BOLD);
    }

    // 빈 라인 출력 (마지막 행)
    for(int i=0; i<COLS; ++i) mvprintw(cur_row, i, " ");
}

int cmp0(const void *p1, const void *p2){
    return ((proc *)p1)->cpu_usage < ((proc *)p2)->cpu_usage;
}

int cmp1(const void *p1, const void *p2){
    return ((proc *)p1)->mem_usage < ((proc *)p2)->mem_usage;
}

int cmp2(const void *p1, const void *p2){
    return ((proc *)p1)->total_time < ((proc *)p2)->total_time;
}
