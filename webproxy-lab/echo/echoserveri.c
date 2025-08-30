#include "csapp.h"
void echo(int connfd);
int main(int argc, char** argv)
{
    int listenfd, connfd;                   // 리스닝 소켓과 연결 소켓의 파일 디스크립터
    socklen_t clientlen;                    // 클라이언트 주소 구조체의 크기를 저장할 변수
    struct sockaddr_storage clientaddr;     // 클라이언트 주소 정보를 담을 구조체(IPv4/IPv6 모두 호환)
    char client_hostname[MAXLINE], client_port[MAXLINE];    // 클라이언트 호스트 이름과 포트 번호를 저장할 문자열 버퍼
    char* port;
    // 프로그램 실행 시 인자 개수가 2개가 아니면(e.g., ./echoserver port) 사용법을 출력하고 종료
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    
    /* 1. 리스닝 소켓 생성
    Open_listenfd는 사용자가 입력한 포트(argv[1])에서 클라이언트의 연결을 기다리는
    리스닝 소켓을 생성하고, 그 디스크립터(listenfd)를 반환한다.
    */
    port = argv[1];
    listenfd = Open_listenfd(port);

    // 2. 메인 루프: 클라이언트의 연결 요청을 무한히 기다림
    while(1){
        clientlen = sizeof(struct sockaddr_storage);    // 클라이언트 주소 구조체의 크기를 초기화
        /* 2-1. 연결 요청 수락
        Accept 함수는 클라이언트의 연결 요청이 올 때까지 대기(block)한다.
        연결이 수립되면, 클라이언트와 통신할 새로운 연결 소켓(connfd)을 생성하여 반환하고,
        clientaddr 구조체에 접속한 클라이언트의 주소 정보를 채워넣는다.
        */
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* 2-2. 클라이언트 정보 확인 (로깅 목적)
        Getnameinfo 함수는 clientaddr에 담긴 바이너리 주소 정보를 사람이 읽을 수 있는
        호스트 이름(cliemt_hostname)과 포트 번호(client_port) 문자열로 변환한다.
        */
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        /* 2-3. 에코 서비스 수행
        실제 데이터 통신을 처리하는 echo 함수를 호출한다.
        이 함수 안에서 클라이언트가 보낸 데이터를 읽고 그대로 다시 보내주는 작업을 수행한다.
        */
        echo(connfd);

        /* 2-4. 연결 종료
        echo 함수가 끝나면(클라이언트와의 통신이 끝나면) 연결 소켓을 닫는다.
        리스닝 소켓(listenfd)은 닫지 않으므로, 다음클라이언트의 연결을 계속 받을 수 있다. */
        Close(connfd);
    }
    exit(0);
}