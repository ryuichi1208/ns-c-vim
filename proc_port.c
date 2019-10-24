#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#ifndef SOCKET
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#define PROGRAM_NAME "httpd_dayo"
#define BASE_NUM    10
#define DEFAULT_PORT 8080
#define DEFAULT_PROC_NUM 1
#define DEFAULT_TIME_OUT 60
#define PROTCOL_TYPE tcp

#define MAX(a, b) (a > b ? a : b)

typedef unsigned long long u_64;
typedef unsigned int u_32;
typedef unsigned short u_16;

typedef struct {
    int port;
    int proc_num;
    uid_t uid;
    pid_t pid;
    pid_t ppid;
} proc_struct;

static pid_t *c_pid_list;

void usage(char *msg)
{
    if (msg)
        fprintf(stderr, "%s\n", msg);

    fprintf(stdout, "Usage : %s -p [PORT_NUM] -n [PROC_NUM]\n", PROGRAM_NAME);
    exit(1);
}

void sigint_handler(int sig, siginfo_t *info, void *ctx)
{
    return;
}

void set_proc_info(proc_struct *ps)
{
    pid_t pid, ppid;
    ps->pid = getpid();
    ps->ppid = getppid();
    ps->uid = getuid();
    return;
}

int opt_parse(int argc, char **argv, proc_struct* ps)
{
    int i, port, proc_num, opt = 0;
    char *cparm, *dparam = NULL;
    char *endptr;

    set_proc_info(ps);
    ps->port = DEFAULT_PORT;
    ps->proc_num = DEFAULT_PROC_NUM;

    while ((opt = getopt(argc, argv, "p:n:")) != -1) {
        switch (opt) {
            case 'p':
                port = 1;
                ps->port = strtoul(optarg, &endptr, BASE_NUM);
                if (*endptr != '\0' || (ps->port == INT_MAX && errno == ERANGE)) {
                    usage("port invalid number\n");
                }
                break;
            case 'n':
                proc_num = 1;
                ps->proc_num = strtoul(optarg, &endptr, BASE_NUM);
                if (*endptr != '\0' || (ps->proc_num == INT_MAX && errno == ERANGE)) {
                    usage("proc_num invalid number\n");
                } else if (ps->proc_num > 10)
                    usage("proc_num invalid too many!!");
                break;
            default:
                break;
        }
    }
    return 0;
}

int port_binding(proc_struct *ps)
{
    int serv_sock;
    int client_sock;
    struct sockaddr_in serv_sock_addr;
    u_16 serv_port;

    if (( serv_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        usage("socket() failed.");

    memset(&serv_sock_addr, 0, sizeof(serv_sock_addr));
    serv_sock_addr.sin_family = AF_INET;
    serv_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_sock_addr.sin_port = htons(ps->port);

    if (bind(serv_sock, (struct sockaddr *)&serv_sock_addr, sizeof(serv_sock_addr)) < 0)
        usage("bind() failed.");

    if (listen(serv_sock, DEFAULT_TIME_OUT) < 0) {
        usage("listen() failed.");
    }

    return serv_sock;
}

void continue_use_port(int serv_sock, int proc_num)
{
    int client_sock;
    int status;
    u_32 client_len;
    pid_t pid;
    pid_t result;
    struct sockaddr_in client_sock_addr;

    // pidを記録しておく領域を確保
    c_pid_list = malloc(sizeof(pid_t) * proc_num);

    for (int i = 0; i < proc_num; i++) {
        if ((pid = fork()) == -1)
            usage("fork() failed.");

        if (!pid) {
            client_len = sizeof(client_sock_addr);
            if ((client_sock = accept(serv_sock, (struct sockaddr *)&client_sock_addr, &client_len)) <0)
                usage("accept() failed.");

            printf("connected from %s.\n", inet_ntoa(client_sock_addr.sin_addr));

            close(client_sock);
        } else {
            *(c_pid_list + i) = pid;
            fprintf(stdout, "%d\n", *(c_pid_list + i));
            continue;
        }
    }
    // 子プロセスを待ち合わせる関数
    result = wait(&status);
    return;
}

void set_signal_handler()
{
    struct sigaction sa_sigint;
    memset(&sa_sigint, 0, sizeof(sa_sigint));
    sa_sigint.sa_sigaction = sigint_handler;
    sa_sigint.sa_flags = SA_SIGINFO;

    if (sigaction(SIGINT, &sa_sigint, NULL) <0 )
        usage("sigaction() failed.");

    return;
}

int main(int argc, char **argv)
{
    int serv_sock = 0;
    proc_struct *ps = malloc(sizeof(proc_struct));

    /* シグナルハンドラの設定 */
    set_signal_handler();

    /* オプション解析 */
    opt_parse(argc, argv, ps);
    fprintf(stdout, "UID PORT PROC_NUM PID  PPID\n");
    fprintf(stdout, "%3d %-4d %-8d %d %d\n", ps->uid, ps->port, ps->proc_num, ps->pid, ps->ppid);
    fflush(stdout);

    /* 指定ポートのソケットを生成 */
    if (!(serv_sock = port_binding(ps)))
        usage("port_binding() faild.");

    /* ポートをlistenし続ける */
    continue_use_port(serv_sock, ps->proc_num);

    return 0;
}
