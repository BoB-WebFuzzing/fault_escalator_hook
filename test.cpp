#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>

int main() {
    // 프로그램의 PID 가져오기
    pid_t pid = getpid();

    // /tmp/httpreqr.pid 파일 열기 (있으면 덮어씁니다)
    int fd = open("/tmp/httpreqr.pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Failed to open /tmp/httpreqr.pid");
        exit(EXIT_FAILURE);
    }

    // PID를 파일에 쓰기
    if (dprintf(fd, "%d\n", pid) == -1) {
        perror("Failed to write PID to /tmp/httpreqr.pid");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // 파일 닫기
    close(fd);

    printf("PID %d written to /tmp/httpreqr.pid\n", pid);

    // SIGSEGV를 기다리는 메시지 출력
    printf("Waiting for SIGSEGV...\n");

    // SIGSEGV 신호를 대기
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGSEGV);

    int sig;
    if (sigwait(&sigset, &sig) != 0) {
        perror("sigwait");
        exit(EXIT_FAILURE);
    }

    // SIGSEGV 신호가 도착하면 메시지 출력 후 종료
    printf("Received SIGSEGV signal. Exiting.\n");

    return 0;
}
