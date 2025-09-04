#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main()
{
  printf("%s", user_agent_hdr);
  return 0;
}



/*
* tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
* GET method to serve static and dynamic content
*/
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  // 리스닝 소켓 생성
  listenfd = Open_listenfd(argv[1]);  // argv[1] = port
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
  }
}
/* doit - 한 개의 HTTP 트랜잭션을 처리하는 함수 */
void doit(int fd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    
    /*  요청 라인과 헤더 읽기 */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    // 읽어들인 요청 라인을 method, uri, version 세 부분으로 파싱
    sscanf(buf, "%s %s %s", method, uri, version);

    if(strcasecmp(method, "GET")!=0 && strcasecmp(method, "HEAD")!=0){
      clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
      return;
    }

    // 요청 헤더만 읽고 무시합니다.
    read_requesthdrs(&rio);
}

/* clienterror - 클라이언트에게 HTTP 오류 응답을 보낸다. */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
    // HTTP 응답 헤더와 본문을 만들기 위한 임시 버퍼
    char buf[MAXLINE], body[MAXBUF];

    /* 1. HTTP 응답 본문(Body) 만들기 - 브라우저 화면에 표시될 HTML 내용 */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
    sprintf(buf, "Connection: close\r\n");

    Rio_writen(fd, buf, strlen(buf));
    /* 2. HTTP 응답 전송하기 - 실제 네트워크로 보낼 메시지 포장 및 발송 */
    
    // (a) 응답 라인 전송
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));

    // (b) 응답 헤더 전송
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));  // 마지막 줄에는 빈 줄을 하나 추가
    Rio_writen(fd, buf, strlen(buf));

    // (c) 응답 본문 전송
    Rio_writen(fd, body, strlen(body));
}

/* read_requesthdrs - HTTP 요청 헤더를 읽고 무시한다. */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);
  // 읽어온 줄이 ("\r\n")이 아닐 동안 루프
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* parse_uri - URI를 파싱하여 filename과 cgiargs를 설정하고,
정적 콘텐츠면 1, 동적 콘텐츠면 0을 반환하는 함수 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  // 1. 정적 콘텐츠 처리 경로
  if(!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    char *is_adder = &(uri[strlen(uri)-1]);
    
    if(uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    if(is_adder == "adder")
      strcat(filename, "adder.html");
    
    return 1;
  }
  // 2. 동적 콘텐츠 처리 경로
  else{
    // URI에서 '?' 문자를 찾아 CGI 인자의 시작 위치를 찾습니다.
    ptr = strstr(uri, '?');
    if(ptr){  // '?'문자를 찾았다면(CGI 인자가 있다면)
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else  // '?' 가 없다면(CGI 인자가 없다면)
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    // ex) /cgi-bin/adder -> ./cgi-bin/adder
    strcat(filename, uri);
    return 0;
  }
}