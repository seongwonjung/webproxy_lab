/*
tiny.c - A simple, iterative HTTP/1.0 Web server that yses the
        GET method to serve static and dynamic content
*/
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *url, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char* filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void get_head(int fd, char* filename, int filesize);
/*
* tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
* GET method to serve static and dynamic content
*/
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
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
  listenfd = Open_listenfd(argv[1]);  // argv = port
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
    // is_static: 요청이 정적 콘텐츠인지, 동적 콘텐츠인지 나타내는 플래그
    int is_static;
    // sbuf: 파일의 메타데이터(크기, 권한 등)를 저장하기 위한 구조체
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    
    /* 1. 요청 라인과 헤더 읽기 */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    // 읽어들인 요청 라인을 method, uri, version 세 부분으로 파싱
    sscanf(buf, "%s %s %s", method, uri, version);

    // // Tiny 서버는 GET 메서드만 지원합니다.
    // if(strcasecmp(method, "GET")){  // 만약 메서드가 "GET"이 아니라면
    //     // 클라이언트에게 501 에러(구현되지 않음)를 보냅니다.
    //     clienterror(fd, method, "501", "Not Implemented", 
    //                 "Tiny does not implement this method");
    //     return;
    // }
    if(strcasecmp(method, "GET")!=0 && strcasecmp(method, "HEAD")!=0){
      clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
      return;
    }

    // 요청 헤더만 읽고 무시합니다.
    read_requesthdrs(&rio);
    
    /* 2. URI 파싱하여 정적/동적 콘텐츠 구분 및 파일 확인 */
    is_static = parse_uri(uri, filename, cgiargs);

    // stat 함수를 사용해 파일이 디스크에 실제로 존재하는지 확인
    if(stat(filename, &sbuf) < 0){
        // 파일이 존재하지 않으면 404에러(찾을 수 없음)
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }

    /* 3. 콘텐츠 종류에 따라 클라이언트에게 제공 */
    if(is_static){  /* 정적 콘텐츠인 경우 */
        // 파일이 일반 파일(regular file)인지, 그리고 읽기 권한이 있는지 확인합니다.
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
            // 권한이 없으면 403 에러
            clienterror(fd, filename, "403", "Forbidden", 
                        "Tiny couldn't read the file");
            return;
        }
        if(!strcasecmp(method, "HEAD"))
          serve_static(fd, filename, sbuf.st_size);
        else
          serve_static(fd, filename, sbuf.st_size);
    }
    else{   /* 동적 콘텐츠인 경우 */
        // 파일이 일반 파일인지, 그리고 실행 권한이 있는지 확인
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
            // 권한이 없으면 403 에러
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
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
  // 읽어온 줄이 헤더의 끝이 아닐 동안 루프
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
    // 정적 콘텐츠는 CGI 인자가 없으므로 cgiargs를 빈 문자열로 설정
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    char *is_adder = &(uri[strlen(uri)-1]);
    // 만약 URI가 '/'로 끝난다면
    if(uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    if(is_adder == "adder")
      strcat(filename, "adder.html");
    // 정적 콘텐츠 이므로 1 반환
    return 1;
  }
  // 2. 동적 콘텐츠 처리 경로
  else{
    // URI에서 '?' 문자를 찾아 CGI 인자의 시작 위치를 찾습니다.
    ptr = strchr(uri, '?');
    if(ptr){  // '?'문자를 찾았다면(CGI 인자가 있다면)
      // '?' 문자 바로 다음 위치부터 문자열 끝까지를 cgiargs 버퍼에 복사
      strcpy(cgiargs, ptr+1);
      // '?' 위치를 NULL 문자('\0')로 바꿔서, uri 문자열을 파일 경로 부분에서 잘라냅니다.
      *ptr = '\0';
    }
    else  // '?' 가 없다면(CGI 인자가 없다면)
      // cgiargs를 빈 문자열로 설정합니다.
      strcpy(cgiargs, "");
    // CGI 프로그램의 전체 경로를 만듭니다.
    strcpy(filename, ".");
    // ex) /cgi-bin/adder -> ./cgi-bin/adder
    strcat(filename, uri);
    return 0;
  }
}

/* serve_static - 정적 파일을 클라이언트에게 전송합니다. */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;    // 읽어올 파일의 디스크립터
  char *srcp;   // 메모리에 매핑된 파일의 시작 주소를 가리킬 포인터
  char filetype[MAXLINE], buf[MAXBUF];    // 파일 타입과 HTTP 응답 헤더를 만들 버퍼

  /* 1. 클라이언트에게 HTTP 응답 헤더(Response Headers) 전송 */
  // 파일 이름의 확장자를 보고 파일 타입(MIME 타입)을 결정한다.
  get_filetype(filename, filetype);

  // sprintf를 사용해 'buf' 버퍼에 응답 헤더들을 차례로 만든다.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  // 완성된 헤더를 클라이언트에게 전송한다.
  Rio_writen(fd, buf, strlen(buf));
  // (디버깅 목적)
  printf("Response headers:\n");
  printf("%s", buf);

  /* 2. 클라이언트에게 HTTP 응답 본문(Response Body) 전송 */
  // 전송할 파일을 읽기 전용으로 연다. (O_RDONLY)
  srcfd = Open(filename, O_RDONLY, 0);
  
  // Mmap 함수를 호출하여 파일의 내용을 메모리에 매핑한다.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  // Close(srcfd);
  // Rio_writen(fd, srcp, filesize);
  // Munmap(srcp, filesize);

  srcp = Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Rio_writen(fd, srcp, filesize);
  Close(srcfd);
  free(srcp);
}

/* get_filetype - Derive file type from filename */
void get_filetype(char *filename, char *filetype)
{
  if(strstr(filename, ".html")){
    strcpy(filetype, "text/html");
  }
  else if(strstr(filename, ".gif")){
    strcpy(filetype, "image/gif");
  }
  else if(strstr(filename, "png")){
    strcpy(filetype, "image/png");
  }
  else if(strstr(filename, "jpg")){
    strcpy(filetype, "image/jpg");
  }
  else if(strstr(filename, "jpeg")){
    strcpy(filetype, "image/jpeg");
  }
  else if(strstr(filename, "mp4")){
    strcpy(filetype, "video/mp4");
  }
  else{
    strcpy(filetype, "text/plain");
  }
}

/* serve_dynamic - 동적 콘텐츠를 클라이언트에게 제공한다. */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  // HTTP 응답 버퍼, execve에 넘길 빈 인자 목록
  char buf[MAXLINE], *emptylist[] = {NULL};
  /* 1. HTTP 응답의 첫 부분(헤더 일부)을 클라이언트에게 보낸다.
  Content-type이나 Content-length 헤더는 CGI 프로그램이 직접 생성해야 한다.*/
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 자식 프로세스를 생성한다. Fork의 반환값은 자식에서는 0, 부모에서는 자식의 PID.
  if(Fork() == 0){  // 자식 프로세스(Child)
    /* 2. CGI 환경 변수를 설정한다 */
    setenv("QUERY_STRING", cgiargs, 1);
    /* 3. 자식 프로세스의 표준 출력을 클라이언트 소켓으로 리디렉션한다. */
    Dup2(fd, STDOUT_FILENO);
    /* 4. CGI 프로그램을 실행한다 */
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}

/* get_head - 정적 콘텐츠의 head를 클라이언트에서 출력한다. */
void get_head(int fd, char* filename, int filesize){
  char buf[MAXBUF], filetype[MAXLINE];
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);
}


void parse_uri(char *uri, char *hostname, char* port, char *path)
{
  // uri = http://www.cmu.edu:8080/hub/index.html
  char *host_start, *port_start, *path_start;
  int len;
  /*
  어떻게 나눌까?
  //를 기준으로 이후부터 host_start로 놓고
  : 를 port_start, /를 path_start 로 해놓으면?
  hostname -> host_start ~ port_start,
  port -> port_start ~ path_start,
  path -> path_start ~
  */
  host_start = strstr(uri, "//");
  if(host_start == NULL){
    return -1;
  }
    host_start += 2;
    port_start = strchr(host_start, ":");
    path_start = strchr(host_start, "/");
    if(path_start == NULL){
    return -1;
  }
  if(port_start == NULL){
    strcpy(port, "80");
    len = path_start - host_start;
    strncpy(hostname, host_start, len);
    hostname[len] = '\0';
  }else{
    len = path_start - (port_start+1);
    strncpy(port, port_start+1, len);
    port[len] = '\0';
    len = port_start - host_start;
    strncpy(hostname, host_start, len);
    hostname[len] = '\0';
  }
  strcpy(path, path_start);
}