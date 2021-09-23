#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>

typedef struct fsinfo{
    char fs_name[64];
    char fs_type[32];
    char mount_path[64];
    long fs_blocks;
    long fs_avail;
    long fs_used;
} FS;

int main(){
    char buf[256];
    long bsizek;
    struct statfs fs;

    FILE *fp = fopen("/proc/mounts", "r");
    FS *info = (FS *)malloc(sizeof(FS));
    
    while(fgets(buf, sizeof(buf), fp)){ // /proc/mounts 파일에서 정보 가져옴
        sscanf(buf, "%s%s%s", info->fs_name, info->mount_path, info->fs_type);
        if(strstr(info->fs_name, "/dev") != NULL){ // /dev 아래에 있는 디바이스 파일만 확인
            statfs(info->mount_path, &fs);
            bsizek = fs.f_bsize/1024; // 1-Kbyte 로 블록 표현
            info->fs_blocks = fs.f_blocks*bsizek;
            info->fs_avail = fs.f_bavail*bsizek;
            info->fs_used = info->fs_blocks-fs.f_bfree*bsizek;
            printf("%-12s %-12s %12ld %12ld %12ld %-30s\n", info->fs_name, info->fs_type, info->fs_blocks, info->fs_used, info->fs_avail, info->mount_path);
        }
    }
    return 0;
}