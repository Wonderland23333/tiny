/**
 * @file tiny.c
 * @brief tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 * build: gcc -o tiny tiny.c csapp.c -pthread
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 * @version 0.1
 */
#include "csapp.h"
#include<pthread.h>
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(const char *uri, char *filename, char *cgiargs);
void serve_static(int fd, const char *filename, int filesize);
void get_filetype(const char *filename, char *filetype);
void serve_dynamic(int fd, const char *filename, const char *cgiargs);
void clienterror(int fd, const char *cause, const char *errnum,
                 const char *shortmsg, const char *longmsg);

pthread_mutex_t mutex;

void *handle_client(void *arg)
{
    int connfd = *(int *)arg;
    pthread_detach(pthread_self());
    doit(connfd);
    return NULL;

}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    
    int ret = pthread_mutex_init(&mutex, NULL);
    if (ret != 0) {
        printf("pthread_mutex_init failed\n");
        return 1;
    }

    printf("pthread_mutex_init success\n");

    fd_set read_set, ready_set;
    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);//添加标准输入0描述符

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = Open_listenfd(argv[1]);
    FD_SET(listenfd, &read_set);
    int maxfd = listenfd;

    while (1)
    {
        ready_set = read_set;
        Select(maxfd + 1, &ready_set, NULL, NULL, NULL);

        if (FD_ISSET(STDIN_FILENO, &ready_set))
        {
            // 如果按下ctrlC，终止服务器
            printf("Terminating server.\n");
            exit(0);
        }

        if (FD_ISSET(listenfd, &ready_set))
        {
            clientlen = sizeof(clientaddr);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            if (connfd < 0) {
            printf("Invalid file descriptor\n");
            return 0;
            }
            printf("Accepted connection from client.\n");

            // 启动线程处理客户端连接
            Pthread_create(&tid, NULL, handle_client, &connfd);

            // 添加新的connfd到read_set中
            FD_SET(connfd, &read_set);
            if (connfd > maxfd)
                maxfd = connfd;
        }
        else
        {
            // 处理已连接的客户端请求
            int i;
            for (i = listenfd + 1; i <= maxfd; i++)
            {
                if (FD_ISSET(i, &ready_set))
                {
                    connfd = i;
                    if (connfd < 0) {
            printf("Invalid file descriptor\n");
            return 0;
            }
                    doit(connfd);
                    FD_CLR(connfd, &read_set);
                }
            }
        }
    }
    return 0;
}
/* $end tinymain */

/**
 * @brief doit - handle one HTTP request/response transaction
 *
 * @param fd
 */
void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)) // line:netp:doit:readrequest
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); // line:netp:doit:parserequest
    if (strcasecmp(method, "GET") && strcasecmp(method,"POST"))
    { // line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }                       // line:netp:doit:endrequesterr
    read_requesthdrs(&rio); // line:netp:doit:readrequesthdrs

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs); // line:netp:doit:staticcheck
    if (stat(filename, &sbuf) < 0)
    { // line:netp:doit:beginnotfound
        clienterror(fd, filename, "404", "Not found",
                    "Tiny couldn't find this file");
        return;
    } // line:netp:doit:endnotfound

    if (is_static)
    { /* Serve static content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        { // line:netp:doit:readable
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size); // line:netp:doit:servestatic
    }
    else
    { /* Serve dynamic content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        { // line:netp:doit:executable
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't run the CGI program");
            return;
        }
        char s[MAXBUF];
        sprintf(s,"%s",rio.rio_bufptr);
        serve_dynamic(fd, filename, s); // line:netp:doit:servedynamic
    }
}
/* $end doit */

/**
 * @brief read_requesthdrs - read HTTP request headers
 *
 * @param rp
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while (strcmp(buf, "\r\n"))
    { // line:netp:readhdrs:checkterm
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

/**
 * @brief parse_uri - parse URI into filename and CGI args
 *
 * @param uri
 * @param filename
 * @param cgiargs
 * @return int
 *          return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(const char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin"))
    { /* Static content */                 // line:netp:parseuri:isstatic
        strcpy(cgiargs, "");               // line:netp:parseuri:clearcgi
        strcpy(filename, ".");             // line:netp:parseuri:beginconvert1
        strcat(filename, uri);             // line:netp:parseuri:endconvert1
        if (uri[strlen(uri) - 1] == '/')   // line:netp:parseuri:slashcheck
            strcat(filename, "test.html"); // line:netp:parseuri:appenddefault
        return 1;
    }
    else
    { /* Dynamic content */    // line:netp:parseuri:isdynamic
        ptr = index(uri, '?'); // line:netp:parseuri:beginextract
        if (ptr)
        {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, ""); // line:netp:parseuri:endextract
        strcpy(filename, ".");   // line:netp:parseuri:beginconvert2
        strcat(filename, uri);   // line:netp:parseuri:endconvert2
        return 0;
    }
}
/* $end parse_uri */

/**
 * @brief serve_static - copy a file back to the client
 *
 * @param fd
 * @param filename
 * @param filesize
 */
void serve_static(int fd, const char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);    // line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf)); // line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);                        // line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // line:netp:servestatic:mmap
    Close(srcfd);                                               // line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);                             // line:netp:servestatic:write
    Munmap(srcp, filesize);                                     // line:netp:servestatic:munmap
}

/**
 * @brief get_filetype - derive file type from file name
 *
 * @param filename
 * @param filetype
 */
void get_filetype(const char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}
/* $end serve_static */

/**
 * @brief serve_dynamic - run a CGI program on behalf of the client
 *
 * @param fd
 * @param filename
 * @param cgiargs
 */
void serve_dynamic(int fd, const char *filename, const char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = {NULL};

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0)
    { /* Child */ // line:netp:servedynamic:fork
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1);                         // line:netp:servedynamic:setenv
        Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */    // line:netp:servedynamic:dup2
        Execve(filename, emptylist, environ); /* Run CGI program */ // line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ // line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/**
 * @brief clienterror - returns an error message to the client
 *
 * @param fd
 * @param cause
 * @param errnum
 * @param shortmsg
 * @param longmsg
 */
void clienterror(int fd, const char *cause, const char *errnum,
                 const char *shortmsg, const char *longmsg)
{
    char buf[MAXLINE];
    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor="
                 "ffffff"
                 ">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "</p></body></html>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */
