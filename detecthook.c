#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>


#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

int (*original_openat)(int dirfd, const char *pathname, int flags, ...);

static ssize_t (*real_recv)(int sockfd, void *buf, size_t len, int flags) = NULL;
static ssize_t (*real_write)(int fd, const void *buf, size_t count) = NULL;
static ssize_t (*real_read)(int fd, void *buf, size_t count) = NULL;

bool pattern_in_bytes(unsigned char *target, int target_len, unsigned char *pattern, int pattern_len);
void SendSignal();
int jdbc_error_check(unsigned char *cptr, size_t len);

void SendSignal()
{
    printf("pre Send Signal");
    // 시그널 전송
    FILE *pidFile = fopen("/tmp/httpreqr.pid", "r");
    if (pidFile)
    {
        char pidStr[10];
        if (fgets(pidStr, sizeof(pidStr), pidFile))
        {
            int pid = atoi(pidStr);
            // PID에 Segment Fault 신호 보내기
            printf("Send Signal : %d\n", pid);
            kill(pid, SIGSEGV);
            // printf("Sent SIGSEGV signal to PID %d\n", pid);
        }
        fclose(pidFile);
    }
    else
        printf("can't open httpreqr.pid");

}

int jdbc_error_check(unsigned char *cptr, size_t len)
{
    // JDBC 오류 메시지 패턴 확인
    unsigned char *jdbc_msg1 = "\x02\x00\x00\x00.\x00\x00\x00.~\x00\x00\x00\x05~\xff\xff\xea";
    unsigned char *jdbc_msg4 = "\xff\xff\xea";

    if (pattern_in_bytes(cptr, len, jdbc_msg4, 3))
    {
        if (pattern_in_bytes(cptr, len, jdbc_msg1, 18))
        {
            return 1;
        }
    }

    return 0;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    real_recv = dlsym(RTLD_NEXT, "recv");
    ssize_t result = real_recv(sockfd, buf, len, flags);

    unsigned char *cptr = (unsigned char *)(buf);
    unsigned char *mysql_msg = "You have an error i";
    int error_msg_len = strlen(mysql_msg);

    unsigned char *jdbc_msg = "java.sql.SQLSyntaxErrorException:";
    int jbdc_msg_len = strlen(jdbc_msg);

    unsigned char *nosql_msg = "BadValue";
    int nosql_msg_len = strlen(nosql_msg);

    unsigned char *postsql_msg_1 = "SERROR";
    int postsql_msg_1_len = strlen(postsql_msg_1);

    unsigned char *postsql_msg_2 = "VERROR";
    int postsql_msg_2_len = strlen(postsql_msg_2);

    unsigned char *postsql_msg_3 = "or near";
    int postsql_msg_3_len = strlen(postsql_msg_3);

    FILE *file = fopen("/tmp/readhook", "a");
    if (file)
    {
        fwrite(buf, 1, result, file);
        fclose(file);
    }

    if (pattern_in_bytes(cptr, result, mysql_msg, error_msg_len))
    {
        printf("recv detected! 1\n");
        SendSignal();
    }
    else if (pattern_in_bytes(cptr, result, jdbc_msg, jbdc_msg_len))
    {
        printf("recv detected try jdbc\n");
        SendSignal();
    }
    else if (pattern_in_bytes(cptr, result, nosql_msg, nosql_msg_len))
    {
        printf("recv detected try nosql\n");
        SendSignal();
    }
    else if (pattern_in_bytes(cptr, result, postsql_msg_1, postsql_msg_1_len) &&
             pattern_in_bytes(cptr, result, postsql_msg_2, postsql_msg_2_len) &&
             pattern_in_bytes(cptr, result, postsql_msg_3, postsql_msg_3_len))
    {
        printf("recv detected postsql\n");
        SendSignal();
    }
    else if (jdbc_error_check(cptr, result))
    {
        printf("recv detected! 2\n");
        SendSignal();
    }

    return result;
}

typedef int (*connect_type)(int, const struct sockaddr *, socklen_t);

// connect 함수 후킹
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    // 원래의 connect 함수 가져오기
    connect_type orig_connect = (connect_type)dlsym(RTLD_NEXT, "connect");

    // IP 주소와 포트 번호 가져오기
    char ip_address[INET_ADDRSTRLEN];
    struct sockaddr_in *saddr = (struct sockaddr_in *)addr;
    inet_ntop(AF_INET, &(saddr->sin_addr), ip_address, INET_ADDRSTRLEN);
    int port = ntohs(saddr->sin_port);

    const char *target_ip = "127.0.0.1";
    const int target_port = 5000;

    // IP 주소와 포트 번호가 원하는 것과 같을 때 출력
    if (strcmp(ip_address, target_ip) == 0 && port == target_port){
        printf("connect detected! \n");
        SendSignal();
    }

    // 원래의 connect 함수 호출
    return orig_connect(sockfd, addr, addrlen);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    real_write = dlsym(RTLD_NEXT, "write");
    ssize_t result = real_write(fd, buf, count);
    unsigned char *cptr = (unsigned char *)(buf);
    unsigned char *sqlite_msg = "SQLITE_ERROR:";
    int sqlite_msg_len = strlen(sqlite_msg);

    // 로그 전송
    FILE *file = fopen("/tmp/writehook", "a");
    if (file)
    {
        fwrite(buf, 1, result, file);
        fclose(file);
    }

    unsigned char *jdbc_msg = "java.sql.SQLSyntaxErrorException:";
    int jbdc_msg_len = strlen(jdbc_msg);

    if (pattern_in_bytes(cptr, result, sqlite_msg, sqlite_msg_len))
    {
        SendSignal();
    }
    else if (pattern_in_bytes(cptr, result, jdbc_msg, jbdc_msg_len))
    {
        printf("write detected try jdbc\n");
        SendSignal();
    }

    return result;
}

ssize_t read(int fd, void *buf, size_t count)
{
    real_read = dlsym(RTLD_NEXT, "read");
    ssize_t result = real_read(fd, buf, count);
    unsigned char *cptr = (unsigned char *)(buf);
    unsigned char *mysql_msg = "You have an error i";
    int error_msg_len = strlen(mysql_msg);

    unsigned char *jdbc_msg = "java.sql.SQLSyntaxErrorException:";
    int jbdc_msg_len = strlen(jdbc_msg);

    unsigned char *nosql_msg = "BadValue";
    int nosql_msg_len = strlen(nosql_msg);

    unsigned char *postsql_msg_1 = "SERROR";
    int postsql_msg_1_len = strlen(postsql_msg_1);

    unsigned char *postsql_msg_2 = "VERROR";
    int postsql_msg_2_len = strlen(postsql_msg_2);

    unsigned char *postsql_msg_3 = "or near";
    int postsql_msg_3_len = strlen(postsql_msg_3);

    FILE *file = fopen("/tmp/readhook", "a");
    if (file)
    {
        fwrite(buf, 1, result, file);
        fclose(file);
    }

    if (pattern_in_bytes(cptr, count, mysql_msg, error_msg_len))
    {
        printf("read detected! 1\n");
        SendSignal();
    }
    else if (pattern_in_bytes(cptr, count, jdbc_msg, jbdc_msg_len))
    {
        printf("read detected try jdbc\n");
        SendSignal();
    }
    else if (pattern_in_bytes(cptr, count, nosql_msg, nosql_msg_len))
    {
        printf("read detected try nosql\n");
        SendSignal();
    }
    else if (pattern_in_bytes(cptr, count, postsql_msg_1, postsql_msg_1_len) &&
             pattern_in_bytes(cptr, count, postsql_msg_2, postsql_msg_2_len) &&
             pattern_in_bytes(cptr, count, postsql_msg_3, postsql_msg_3_len))
    {
        printf("read detected postsql\n");
        SendSignal();
    }
    else if (jdbc_error_check(cptr, count))
    {
        printf("read detected! 2\n");
        SendSignal();
    }

    return result;
}


#define CHECK_FILE_VULN "/etc/passwd"

// 파일 열기 플래그에 대한 상수 정의
#define O_CREAT 0100

int open(const char *pathname, int flags, ...) {
    int (*original_open)(const char *, int, ...);
    original_open = dlsym(RTLD_NEXT, "open");

    // 후킹할 동작 추가
    printf("Intercepted open of file: %s\n", pathname);

    // "/etc/passwd" 확인
    if (strstr(pathname, CHECK_FILE_VULN) != NULL) {
        printf("%s Done\n", pathname);
        SendSignal();
    }

    // 원래의 open 함수 호출
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        return original_open(pathname, flags, mode);
    } else {
        return original_open(pathname, flags);
    }
}


/*
int openat(int dirfd, const char *pathname, int flags, ...)
{
    // 원본 openat 함수를 불러옵니다.
    if (!original_openat)
    {
        original_openat = dlsym(RTLD_NEXT, "openat");
    }

    va_list args;
    va_start(args, flags);
    mode_t mode = va_arg(args, mode_t);
    va_end(args);

    // 원하는 후킹 동작을 수행할 수 있습니다.
    // 예를 들어, 파일 경로를 변경하거나 로깅을 추가할 수 있습니다.
    // printf("Hooked openat: %s\n", pathname);

    int size = strlen(pathname);
    FILE *file = fopen("/tmp/openathook", "a");
    if (file)
    {
        fwrite(pathname, 1, size, file);
        fclose(file);
    }

    // 원본 openat 함수를 호출합니다.
    return original_openat(dirfd, pathname, flags, mode);
}
*/
bool pattern_in_bytes(unsigned char *target, int target_len, unsigned char *pattern, int pattern_len)
{
    if (target_len <= pattern_len)
    {
        return false;
    }
    for (int i = 0; i < target_len - pattern_len; i++)
    {
        bool found = true;
        for (int j = 0; j < pattern_len; j++)
        {

            if (pattern[j] == '.')
            {
                i++;
                continue;
            }
            else if (pattern[j] == '~')
            {
                if (target[i] >= 0x20 && target[i] < 0x7f)
                {
                    while (target[i] >= 0x20 && target[i] < 0x7f)
                    {
                        i++;
                    }
                    continue;
                    found = false;
                    break;
                }
            }

            if (target[i] != pattern[j])
            {
                found = false;
                break;
            }

            i++;
        }
        if (found)
        {
            return true;
        }
    }

    return false;
}

// gcc -shared -o hook_recv.so testhook.c -ldl -fPIC
// LD_PRELOAD=./hook_recv.so node app.js
