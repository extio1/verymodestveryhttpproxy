#ifndef OS_PROXY_LOG_H
#define OS_PROXY_LOG_H

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#define logfile stdout

#define INFO "INFO"
#define ERROR "ERROR"

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static time_t now_logger_time;
static struct tm tm_now;
static char buff[100]; 
#define log(tag, ...)                                                                      \
                if( pthread_mutex_lock(&log_mutex) != 0){                                  \
                    perror("logger pthread_mutex_lock");                                   \
                }                                                                          \
                now_logger_time = time(NULL);                                              \
                localtime_r(&now_logger_time, &tm_now);                                    \
                strftime(buff, sizeof(buff), "%H:%M:%S -- %Y-%m-%d", &tm_now);             \
                if(strcmp(tag, ERROR) == 0){                                               \
                    fprintf(logfile, "\e[36m%s\e[0m [ \e[1;31m%s\e[0m ]: ", buff, tag);    \
                    fprintf(logfile, __VA_ARGS__);                                         \
                    fprintf(logfile, "\n");                                                \
                    fprintf(logfile, "%s %d", __FILE__, __LINE__);                         \
                } else {                                                                   \
                    fprintf(logfile, "\e[36m%s\e[0m [ \e[92m%s\e[0m ] : ", buff, tag);     \
                    fprintf(logfile, __VA_ARGS__);                                         \
                }                                                                          \
                fprintf(logfile, "\n");                                                    \
                if( pthread_mutex_unlock(&log_mutex) != 0){                                \
                    perror("logger pthread_mutex_unlock");                                 \
                }                                                                          \

#endif /* OS_PROXY_LOG_H */