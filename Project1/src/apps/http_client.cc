#include "minet_socket.h"
#include <stdlib.h>
#include <iostream>
#include <ctype.h>

using namespace std;

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int fail_and_exit(int sock, const char * errorMsg);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    //char * bptr = NULL;
    //char * bptr2 = NULL;
    //char * endheaders = NULL;

    //struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
    fprintf(stderr, "usage: http_client k|u server port path\n");
    exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];

    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') {
    minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') {
    minet_init(MINET_USER);
    } else {
    fprintf(stderr, "First argument must be k or u\n");
    exit(-1);
    }

    /* create socket */
    sock = minet_socket(SOCK_STREAM);//

    // Do DNS lookup
    /* Hint: use gethostbyname() */
    site = gethostbyname(server_name);

    if (site == NULL)
    {
        fail_and_exit(sock, "Failed to gethostbyname\n");
    }

    /* set address */
    memset(&sa, 0, sizeof(sa));

    sa.sin_family = AF_INET;
    memcpy((char*) &(sa.sin_addr), (char*) site->h_addr, site->h_length);
    sa.sin_port = htons(server_port);

    /* connect socket */
    if (minet_connect(sock, &sa) != 0){
        fail_and_exit(sock, "Did not connect to socket\n");
    };

    /* send request */
    req = (char *)malloc(strlen(server_path) + 15);
    sprintf(req, "GET %s HTTP/1.0\n\n", server_path);
    if (write_n_bytes(sock, req, strlen(req)) < 0)
    {
        free(req);
        fail_and_exit(sock, "Failed to write request\n");
    }
    free(req);

    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    FD_ZERO(&set);
    FD_SET(sock, &set);
    // if (!FD_ISSET(sock, &set))
    // {
    //     fail_and_exit(sock, "Socket not set\n");
    // }

    if (minet_select(sock+1,&set,NULL,NULL,NULL) < 1) {
        fail_and_exit(sock, "Select socket error\n");
    }

    /* first read loop -- read headers */
    if (minet_read(sock, buf, BUFSIZE) < 0)
    {
        fail_and_exit(sock, "Failed to read\n");
    }

    /* examine return code */
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200

    char* cut = buf;

    while (cut[-1] != ' ')
        cut++;

    char tmp[4];
    strncpy(tmp, cut, 3);
    tmp[4] = '\0';

    rc = atoi(tmp);

    if (rc != 200 && (rc < 300 || rc >= 400))
        wheretoprint = stderr;

    fprintf(wheretoprint, "Status: %d\n\n", rc);

    while (cut[-1] != '\n')
        cut++;

    /* print first part of response */
    while (!(cut[-2] == '\n' && cut[0] == '\n'))
    {
        fprintf(wheretoprint, "%c", cut[0]);
        cut++;
    }

    fprintf(wheretoprint, "\n\nHTML RESPONSE BODY:\n\n");
    fprintf(wheretoprint, "%s", cut);

    datalen = minet_read(sock, buf, BUFSIZE);
    while (datalen > 0) {
        buf[datalen] = '\0';
        fprintf(wheretoprint, "%s", buf);
        datalen = minet_read(sock, buf, BUFSIZE);
    }

    /*close socket and deinitialize */
    minet_close(sock);
    return 0;
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
    totalwritten += rc;
    }

    if (rc < 0) {
    return -1;
    } else {
    return totalwritten;
    }
}

int fail_and_exit(int sock, const char * errorMsg) {
    fprintf(stderr,errorMsg);
    minet_close(sock);
    exit(-1);
}


