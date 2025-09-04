/*
프록시 -> 클라이언트와 서버 사이의 중계자 역할을 한다.
forword proxy 
클라이언트 요청 -> 프록시 -> 서버,
서버 응답 -> 프록시 -> 클라이언트

우선 클라이언트의 요청을 받아야 하므로 listenfd를 만들어서 클라이언트와 연결되어야 할 듯?
그런 다음 요청을 받고 파싱한 다음에 서버로 보내는 역할을 하면 된다.
그럼 서버에서 프록시의 요청을 받고, 처리해서 다시 프록시에 돌려줄거고,
프록시는 서버로부터 받은 응답을 다시 클라이언트에 돌려주면 끝.
*/

#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void assemble_request(rio_t *rp, char *hostname, char *method, char *path, char *version ,char *request_buf);
void parse_uri(char *uri, char *hostname, char* port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
  }
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd)
{
  int clientfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
  char request_buf[MAXLINE], client_buf[MAXLINE];
  rio_t rio, rio_serv;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;

  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("DEBUG: URI being passed to parse_uri: [%s]\n", uri);

  if(strcmp(version, "HTTP/1.0")){
    strcpy(version, "HTTP/1.0");
  }
  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
    return;
  }
  // uri 파싱
  parse_uri(uri, hostname, port, path);
  printf("DEBUG: Parsed hostname=[%s], port=[%s]\n", hostname, port);
  // 이제 request_buf 조립해서 보내자
  assemble_request(&rio, hostname, method, path, version, request_buf);
  clientfd = Open_clientfd(hostname, port);
  Rio_writen(clientfd, request_buf, strlen(request_buf));
  // 응답 받아서 클라한테 보내자
  Rio_readinitb(&rio_serv, clientfd);
  int n = 0;
  while((n = Rio_readnb(&rio_serv, client_buf, MAXLINE)) != 0){
    Rio_writen(fd, client_buf, n);
  }
}

/* 서버로 보낼 HTTP 요청(헤더 포함) 조립  */
void assemble_request(rio_t *rp, char *hostname, char *method, char *path, char *version ,char *request_buf)
{
  char buf[MAXLINE];
  char host_hdr[MAXLINE];
  int is_host = 0;

  sprintf(request_buf, "%s %s %s\r\n", method, path, version);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while (strcmp(buf, "\r\n"))
  {
    printf("%s", buf);
    if(strstr(buf, "Host:")){
      is_host = 1;
      strcat(request_buf, buf);
    }
    else if (!(strstr(buf, "Connection:") || strstr(buf, "Proxy-Connection:") || strstr(buf, "User-Agent:")))
    {
        strcat(request_buf, buf);
    }
    Rio_readlineb(rp, buf, MAXLINE);
  }
  if(!is_host){
    sprintf(host_hdr, "Host: %s\r\n", hostname);
    strcat(request_buf, host_hdr);
  }
  // Connection, Proxy-Connection -> 무조건 close
  // User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3
  strcat(request_buf, "Connection: close\r\n");
  strcat(request_buf, "Proxy-Connection: close\r\n");
  strcat(request_buf, "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n");
  strcat(request_buf, "\r\n");
  return;
}

/* uri에서 hostname, port, path 파싱 */
void parse_uri(char *uri, char *hostname, char* port, char *path)
{
  // uri = http://www.cmu.edu:8080/hub/index.html
  char *host_start, *port_start, *path_start;
  char buf[MAXBUF];
  strcpy(buf, uri);
  // host_start -> www.cmu.edu:8080/hub/index.html
  host_start = strstr(uri, "http://");
  if(host_start != NULL){
    host_start += 7;
  }else{
    host_start = buf;
  }
  // path_start = /hub/index.html
  path_start = strstr(host_start, "/");
  if(path_start != NULL){
    strcpy(path, path_start);
    *path_start = '\0';
  }else{
    strcpy(path, "/");
  }
  // port_start -> :80, 복사 할 땐 80만 해야 함
  port_start = strstr(host_start, ":");
  if(port_start != NULL){
    strcpy(port, port_start+1);
    (*port_start) = '\0';
  }else{
    strcpy(port, "80");
  }
  strcpy(hostname, host_start);
}



/* clienterror - returns an error message to the client */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n",body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
