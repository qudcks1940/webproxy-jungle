/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

void sigpipe_handler(int signum) {
    printf("SIGPIPE received. Continuing execution...\n");
}

/* open a listeng socket, and then repeatedly accept a connection request, perform a transation, close end of the connection in a loop*/
// argc는 명령어 인자 개수
// argv는 명령어 인자 배열
// 이 프로그램은 명령어 인자로 포트 번호를 받아서 서버 실행

int main(int argc, char **argv) {
    // 각 변수는 네트워크 소켓 프로그래밍에서 연결을 처리하고 클라이언트와
    // 서버 간의 정보를 저장하는 데에 사용됨.

    //listenfd: 서버가 클라이언트 연결을 대기하는 소켓.
    //connfd: 수락된 클라이언트 연결 소켓.
    //hostname : 연결된 클라이언트의 호스트 이름.
    //port : 클라이언트가 사용한 포트 번호.
    //clientlen : 클라이언트 주소의 크기.
    //clientaddr : 클라이언트의 소켓 주소 정보.

    // 의미: 이 변수는 서버 소켓 파일 디스크립터를 저장
    // 용도: 서버가 클라이언트의 연결을 기다리기 위해 사용하는 소켓
    // Open_listenfd 함수에서 반환되는 값,
    // 서버는 이 소켓을 통해 클라이언트의 연결 요청을 받음.
    // 배경: 이 소켓은 listening 상태에 있으며, 클라이언트가 서버에 연결하려고
    // 시도할 때 그 요청을 감지하고 받아들인다.
    int listenfd;

    // 의미: 이 변수는 클라이언트와의 개별 연결을 
    // 처리하기 위한 소켓 파일 디스크립터를 저장
    // 용도: 클라이언트의 연결 요청이 수락되면 Accept 함수는 새로운 소켓 생성,
    // 그 소켓의 파일 디스크립터를 connfd에 저장.
    // 이후 이 소켓을 통해 클라이언트와 데이터를 주고받는다.
    // 배경: 서버는 listenfd를 사용해 클라이언트의 연결을 기다리지만,
    // 연결이 수락되면 connfd를 통해 각각의 클라이언트와 통신하게 된다.
    int connfd;
    char hostname[MAXLINE], port[MAXLINE];

    // 의미: 이 변수는 클라이언트 주소의 크기 저장
    // 용도: Accept 함수가 클라이언트의 주소 정보를 저장할 때,
    // 그 주소의 크기를 전달해야 한다.
    // clientlen은 이를 관리하는 데 사용됨.
    // 배경: 클라이언트의 주소 구조체(struct sockaddr_storage)가
    // 가변 크기일 수 있기 때문에, clientlen을 사용해 정확한 크기를 전달
    socklen_t clientlen;

    // 의미: 이 구조체는 클라이언트의 소켓 주소 정보를 저장
    // 용도: 클라이언트의 IP주소와 포트 정보 저장.
    // 이 구조체는 IPv4와 IPv6 모두 저장할 수 있는 큰 크기의 주소 구조체이다.
    // 배경: 네트워크 프로그래밍에서는 IPv4와 IPv6 모두 지원할 필요가 있으며,
    // sockaddr_storage는 이 두 가지 주소 체계를 모두 수용할 수 있는 크기
    struct sockaddr_storage clientaddr;

    struct sigaction sa;
    sa.sa_handler = sigpipe_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    /* Check command-line args */
    // 명령어 인자가 2개가 아니면 (즉, 프로그램 이름과 포트 번호)
    // 잘못된 사용법임을 출력하고, 프로그램 종료
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Open_listenfd는 주어진 포트에서 연결을 대기할 수 있는 소켓을 생성하고
    // 반환하는 함수로 추정
    // 이 소켓의 파일 디스크립터를 저장
    listenfd = Open_listenfd(argv[1]); // opening a listening socket
    
    // 무한 루프를 돌며 클라이언트의 연결을 대기
    // Accept 함수는 클라이언트의 연결을 수락하며,
    // 성공 시 새로운 연결을 나타내는 소켓 파일 디스크립터(connfd)를 반환
    // 또한, 연결된 클라이언트 주소 정보를 clientaddr에 저장
    while (1) {
        clientlen = sizeof(clientaddr);

        // Client 쪽에서 connect함수를 호출하면
        // Server 쪽에 있는 accept함수가 호출됨.
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // acceptig a connection request
        
        // 클라이언트의 주소 정보를 Getnameinfo 함수를 통해
        // 클라이언트의 호스트 이름(hostname)과
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // converting socketaddr to form that is available to read

        // 포트 번호(port)로 변환하고 이를 출력. 
        // 이로써 어느 클라이언트가 서버에 접속했는지 알 수 있습니다.
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        // 실제로 클라이언트와 통신하거나 요청을 처리하는 함수
        doit(connfd);   // line:netp:tiny:doit
        // 연결 종료
        Close(connfd);  // line:netp:tiny:close
    }
}

/* handle one HTTP transaction. read and parse the request line and figure out whether it is static or dynamic content */
// 작은 웹 서버 프로그램인 "Tiny"에서 요청을 처리하는 함수
// 클라이언트와 서버 간의 소켓 통신을 통해 
// HTTP 요청을 처리하는 흐름을 설명
void doit(int fd) {
    // 요청된 콘텐츠가 정적인지 동적인이 구분하는 플래그
    // parse_uri 설정
    int is_static;

    // stat 시스템 콜에서 파일의 메타데이터(크기, 권한 등)를
    // 저장하는 구조체
    struct stat sbuf;

    // 클라이언트로부터 받은 HTTP 요청의 라인에서 
    // 각각 메서드(GET 등), URI, HTTP 버전 정보를 저장하는 문자열
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio; // Robust I/O: 버퍼링된 입출력 처리

    /* Read Request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);

    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not implemented", "Tiny couldn't find this file");
        return;
    }
    read_requesthdrs(&rio);

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }

    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
        return;
        }
        serve_static(fd, filename, sbuf.st_size);
    } else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
        return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

// send an HTTP response to the client with the appropriate status code and status message
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
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

/* Read and ignore request headers */
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* Parse an HHTP URI */
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { // static content
    strcpy(cgiargs, ""); // clear the CGI argument string
    strcpy(filename, "."); 
    strcat(filename, uri); // convert the URI into a relative Linux pathname such as ./index.html
    if (uri[strlen(uri)-1] == '/') // if the URL ends with '/'
      strcat(filename, "home.html"); // append the default file name
    return 1;
  } else { // dynamic content
    ptr = index(uri, "?"); // extract any CGI arguments
    if (ptr) {
      strcpy(cgiargs, ptr+1); 
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, "."); // convert the remaining portion of the URI to a relative Linux filename
    strcat(filename, uri); 
    return 0;
  }
}

/* serve static content to a client */
void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n", buf);
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  /* send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  // Rio_writen(fd, srcp, filesize);
  if (rio_writen(fd, srcp, filesize) < 0) {
    // 리턴 값 기반 에러 처리
    fprintf(stderr, "Error writing file body.\n");
    Munmap(srcp, filesize);
    return;
  }
  Munmap(srcp, filesize);
}

/* derive file type from filename */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text.html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4")){
        strcpy(filetype, "video/mp4");
    }
    else
        strcpy(filetype, "image/png");
}

/* serve dynamic content to a client */
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // the child initializes the Q_S environment variable with the CGI arguments from the request URI
    Dup2(fd, STDOUT_FILENO); // redirect stdout to client
    Execve(filename, emptylist, environ); // load and run the CGI program (in the context of the child)
  }
  Wait(NULL); // parent waits for and reaps child when it terminates
}