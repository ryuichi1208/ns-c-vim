#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define MAXLINE 64

ssize_t
readn(int fd, void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nread;
  char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nread = read(fd, ptr, nleft) ) < 0) {
      if (errno == EINTR)
        nread = 0;
      else
        return -1;
    } else if (nread == 0) {
      break;// EOF
    }
    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft);
}

ssize_t
writen(int fd, const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nwritten = write(fd, ptr, nleft) ) <= 0) {
      if (errno == EINTR)
        nwritten = 0;
      else
        return -1;
    }
    nleft -= nwritten;
    ptr += nwritten;
  }
  return n;
}

ssize_t
readline(int fd, void *vptr, size_t maxlen)
{
  ssize_t n, rc;
  char c, *ptr;

  ptr = vptr;
  for (n = 1; n < maxlen; n++) {
again:
    if ( (rc = read(fd, &c, 1) ) == 1) {
      *ptr++ = c;
      if (c == '\n')
        break;
    } else if (rc == 0) {
      if (n == 1)
        return 0;
      else
        break;
    } else {
      if (errno == EINTR)
        goto again;
      return -1;
    }
  }
  *ptr = 0;
  return n;
}


void
str_echo(int sockfd)
{
  ssize_t n;
  char line[MAXLINE];
  for (;;) {
    if ( (n = readline(sockfd, line, MAXLINE) ) == 0) {
      return;
    }
    writen(sockfd, line, n);
  }
}


int
main(){
  int i, maxi, maxfd, listenfd, connfd, sockfd;
  int nready, client[FD_SETSIZE];
  ssize_t n;
  fd_set rset, allset;
  char line[MAXLINE];
  socklen_t clilen;
  struct sockaddr_in cliaddr, servaddr;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(8005);
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

  listen(listenfd, 64);

  maxfd = listenfd;
  maxi = -1;
  for (i = 0; i < FD_SETSIZE; i++)
    client[i] = -1;

  FD_ZERO(&allset);
  FD_SET(listenfd, &allset);

  for (;;) {
    rset = allset;
    nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
    if (FD_ISSET(listenfd, &rset)) {
      clilen = sizeof(cliaddr);
      connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
      for (i = 0; i < FD_SETSIZE; i++) {
        if (client[i] < 0) {
          client[i] = connfd;
          break;
        }
      }
      if (i == FD_SETSIZE) {
        perror("too many clients");
        exit(1);
      }
      FD_SET(connfd, &allset);
      if (connfd > maxfd)
        maxfd = connfd;
      if (i > maxi)
        maxi = i;
      if (-nready <= 0)
        continue;
    }
    for (i = 0; i <= maxi; i++) {
      if ( (sockfd = client[i]) < 0)
        continue;
      if (FD_ISSET(sockfd, &rset)) {
        if ( (n = readline(sockfd, line, MAXLINE))  == 0 ) {

          close(sockfd);
          FD_CLR(sockfd, &allset);
          client[i] = -1;
        } else {
          writen(sockfd, line, n);
        }
        if (-nready <= 0)
          break;
      }
    }
  }

  return 0;
}
