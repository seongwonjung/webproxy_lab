#include "csapp.h"
int main(int argc, char **argv)
{
    int clientfd;                       // 서버와의 연결을 위한 소켓 파일 디스크립터
    char *host, *port, buf[MAXLINE];    // 서버의 호스트(IP 주소)와 포트 번호를 저장할 포인터, 데이터를 읽고 쓰기 위한 버퍼
    rio_t rio;                          // Rio 버퍼링 I/O를 위한 구조체
    
    // 프로그램 실행 시 인자 개수가 3개가 아니면(e.g., ./echoclient host por) 사용법을 출력하고 종료
    if(argc != 3){
        fprintf(stderr, "usage %s <host> <port>\n", argv[0]);
        exit(0);
    }
    // 첫 번째 인자를 host로, 두 번째 인자를 port로 지정
    host = argv[1];
    port = argv[2];
    /* 1. 서버에 연결 시도
    Open_clientfd는 서버의 host와 port 정보를 이용해 연결을 설정하고,
    연결에 성공하면 통신에 사용할 소켓 디스크립터(clientfd)를 반환한다. */
    clientfd = Open_clientfd(host, port);
    /* 2. Rio 읽기 버퍼 초기화
    clientfd를 Rio 읽기 버퍼(rio)와 연결(associate)한다.
    이제 이 rio 구조체를 통해 clientfd로부터 데이터를 버퍼링하여 읽을 수 있게 된다. */
    Rio_readinitb(&rio, clientfd);
    /* 3. 메인 루프: 사용자 입력을 서버로 보내고, 에코를 받아 출력
    Fgets 함수는 표준 입력(stdin, 즉 키보드)으로부터 한 줄을 읽어 buf에 저장한다.
    사용자가 EOF(Ctrl+D)를 입력하여 NULL이 반환될 때까지 루프를 계속한다. */
    while(Fgets(buf, MAXLINE, stdin) != NULL){
        /* 3-1. 서버로 데이터 전송
        사용자가 입력한 문자열(buf)을 서버(clientfd)로 전송한다.
        Rio_writen은 buf의 내용을 strlen(buf) 길이만큼 모두 보낼 것을 보장한다. */
        Rio_writen(clientfd, buf, strlen(buf));

        /* 3-2. 서버로부터 에코 수신
        서버가 다시 보낸 에코 메시지를 한 줄 읽어 buf에 덮어쓴다.
        Rio_readlenb는 rio 버퍼를 통해 데이터를 효율적으로 읽어온다. */
        Rio_readlineb(&rio, buf, MAXLINE);

        /* 3-3. 받은 에코를 화면에 출력
        서버로부터 받은 buf의 내용을 표준 출력(stdout, 즉 화면)에 출력한다. */
        Fputs(buf, stdout);
    }
    // 4. 연결 종료
    // 루프가 끝나면(사용자가 입력을 멈추면) 서버와의 연결을 닫는다.
    Close(clientfd);
    exit(0);
}