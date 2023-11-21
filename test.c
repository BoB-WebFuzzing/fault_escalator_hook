#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h> // 추가: mode_t를 사용하기 위한 헤더 파일
#include <curl/curl.h>


// Function pointer types for the original openat and access functions
typedef int (*original_access)(const char *, int);

// Define function pointers for the original functions
typedef int (*original_open64)(const char *pathname, int flags, ...);

// Function pointer for the original open64 function
original_open64 original_open64_func;
original_access original_access_func;

FILE *file;

int sendHttpRequest(const char *url, const char *cookie, const char *postData, const char *method);
int sendHttpRequest(const char *url, const char *cookie, const char *postData, const char *method)
{
    CURL *curl;
    CURLcode res;

    // CURL 라이브러리 초기화
    curl = curl_easy_init();
    if (curl)
    {
        // URL 설정
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // HTTP 요청 메서드 설정
        if (strcmp(method, "GET") == 0)
        {
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1); // GET 요청 설정
        }
        else if (strcmp(method, "POST") == 0)
        {
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, cookie);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(postData));
        }

        // 요청을 보내고 응답 받기
        res = curl_easy_perform(curl);

        // 요청 및 응답 처리
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return -1; // 오류 발생
        }
        else
        {
            printf("HTTP 요청이 성공적으로 보내졌습니다.\n");
        }

        // CURL 리소스 정리
        curl_easy_cleanup(curl);
        return 0; // 성공
    }

    return -1; // 오류 발생
}

// Hook for openat function

int open64(const char *pathname, int flags, ...)
{
    // You can add your hooking code here
    printf("Hooked open64(%s, %d)\n", pathname, flags);

    // Call the original function with the same arguments
    va_list args;
    va_start(args, flags);
    mode_t mode = va_arg(args, mode_t);
    va_end(args);

    return original_open64_func(pathname, flags, mode);
}
// Hook for access function
int access(const char *pathname, int mode)
{
    // You can add your hooking code here
    printf("access(%s, %d)\n", pathname, mode);

    fprintf(file, "%s\n", pathname);

    char *check_msg = "wtf_test";

    // Check if "wtf_test" is part of the pathname
    if (strstr(pathname, check_msg) != NULL)
    {
        printf("Pathname contains 'wtf_test'\n");

        // file에서 비교
        printf("Send Signal\n");
        return original_access_func(pathname, mode);
    }

    // 비교 packet.out 에서 있는지 알아내야함
    FILE *po = fopen("/tmp/packet.out", "r+");
    if (po)
    {
        printf("Opened /tmp/packet.out\n");

        char buffer[4096];
        char *replace = "wtf_test";
        char *search = pathname;
        int search_len = strlen(search);

        while (fgets(buffer, sizeof(buffer), po))
        {
            char *pos;
            while ((pos = strstr(buffer, search)) != NULL)
            {
                int position = pos - buffer;
                fseek(po, -(strlen(buffer) - position), SEEK_CUR);
                fprintf(po, "%s", replace);
                fseek(po, position + search_len, SEEK_CUR);
            }
        }
        fclose(po);

        // 대체 했으면 curl로 전송해야함
        FILE *po2 = fopen("/tmp/packet.out", "r");
        if (po2)
        {
            printf("Opened edited /tmp/packet.out\n");

            char request[4096] = {
                0,
            };
            fgets(request, sizeof(request), po2);

            // method, url 추출
            char *http_method = strtok(request, " ");
            char *url = strtok(NULL, " \r\n");

            // cookie 추출
            char *cookie = strstr(request, "Cookie:");
            char *cookie_value = strtok(cookie, ":\r\n");

            // POST 요청 바디 값 추출
            char *post_body = strstr(request, "\r\n\r\n");
            if (post_body != NULL)
                post_body += 4;

            printf("HTTP Method: %s\n", http_method);
            printf("URL: %s\n", url);
            printf("Cookie: %s\n", cookie_value);
            printf("POST Body: %s\n", post_body);

            sendHttpRequest(url, cookie_value, post_body, http_method);

            fclose(po2);
        }
    }

    // Call the original function
    return original_access_func(pathname, mode);
}

__attribute__((constructor)) void init()
{
    // Load the original openat and access functions

    original_open64_func = (original_open64)dlsym(RTLD_NEXT, "open64");
    original_access_func = (original_access)dlsym(RTLD_NEXT, "access");
    file = fopen("/tmp/savepath", "w");
}

// gcc -shared -fPIC -o hook.so testhook.c -ldl