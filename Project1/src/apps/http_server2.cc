#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
int fail_and_exit(int sock, const char * errorMsg);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc,i;
  fd_set readlist;
  fd_set connections;
  int maxfd;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server2 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize minet */
  if (toupper(*(argv[1])) == 'K') {
  minet_init(MINET_KERNEL);
  } else if (toupper(*(argv[1])) == 'U') {
  minet_init(MINET_USER);
  } else {
  fprintf(stderr, "First argument must be k or u\n");
  exit(-1);
  }

  /* initialize and make socket */
  sock = minet_socket(SOCK_STREAM);

  /* set server address*/
  memset(&sa, 0, sizeof(sa));

  sa.sin_family = AF_INET;
  sa.sin_port = htons(server_port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);

  /* bind listening socket */
  if (minet_bind(sock, &sa) < 0)
  {
    fail_and_exit(sock, "Could not bind to socket\n");
  }

  /* start listening */
  if (minet_listen(sock, 5) < 0) //queue up to 5 connections...could easily change this #
  {
    fail_and_exit(sock, "Error while listening on socket\n");
  }

  FD_ZERO(&connections);
  FD_SET(sock,&connections);
  maxfd = sock;

  /* connection handling loop */
  while(1)
  {
    /* handle connections */
    /* create read list */
    FD_ZERO(&readlist);
    for (i = 0; i <= maxfd; i++)
    {
      if (FD_ISSET(i,&connections))
      {
        FD_SET(i,&readlist);
      }
    }

    /* do a select */
    if (minet_select(maxfd+1,&readlist,NULL,NULL,NULL) < 1) { //need a timeout??
        fail_and_exit(sock, "Select socket error\n");
    }

    /* process sockets that are ready */
    for (i = 0; i <= maxfd; i++)
    {
      if (FD_ISSET(i,&readlist))
      {
        /* for the accept socket, add accepted connection to connections */
        if (i == sock)
        {
          memset(&sa2, 0, sizeof(sa2));
          if ((sock2 = minet_accept(sock, &sa2)) < 0)
          {
            fail_and_exit(sock, "Error accepting a connection\n");
          }

          FD_SET(sock2, &connections);
          if (sock2 > maxfd)
          {
            maxfd = sock2;
          }
        }
        else /* for a connection socket, handle the connection */
        {
  	       rc = handle_connection(i);

           FD_CLR(i,&connections);
           if (i == maxfd)
           {
              for (int j = maxfd; j >= 0; j--)
              {
                if (FD_ISSET(j,&connections) || j == 0)
                {
                  maxfd = j;
                  break;
                }
              }
           }
        }
      }
    }
  }
}

int handle_connection(int sock2)
{
  //char filename[FILENAMESIZE+1];
  //int rc;
  //int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  //char *headers;
  //char *endheaders;
  //char *bptr;
  int datalen=0;
  const char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  const char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/
  if (minet_read(sock2, buf, BUFSIZE) < 0)
  {
    fail_and_exit(sock2, "Failed to read\n");
  }

  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/

  char *typeOfRequest = strtok(buf," \r\n");
  if (typeOfRequest == NULL || strcasecmp(typeOfRequest, "GET") != 0)
  {
    printf("Ill-formed request - must use GET\n"); //send error response?
  }

  char *filename = strtok(NULL, " \r\n");
  if (filename == NULL)
  {
    printf("Ill-formed request - must specify filename\n"); //send error response?
  }

  char *httpVersion = strtok(NULL," \r\n");
  if (httpVersion == NULL || strcasecmp(httpVersion, "http/1.0") != 0)
  {
    printf("Ill-formed request - must specify correct http version\n"); //send error response?
  }

  /* try opening the file */
  char path[FILENAMESIZE + 1];
  memset(path, 0, FILENAMESIZE + 1); // memset path
  getcwd(path, FILENAMESIZE);
  strncpy(path + strlen(path), filename, strlen(filename));

  char *filedata;

  if(stat(path, &filestat) < 0){
    printf("Error opening file\n");
    ok = false;
  } else {
    datalen = filestat.st_size;

    FILE* myfile = fopen(path, "r");

    filedata = (char *)malloc(datalen);
    memset(filedata, 0, datalen);
    fread(filedata, 1, datalen, myfile);
  }

  /* send response */
  if (ok)
  {
    /* send headers */
    sprintf(ok_response, ok_response_f, datalen);
    if (writenbytes(sock2, ok_response, strlen(ok_response)) < 0)
    {
        fail_and_exit(sock2, "Failed to send response\n");
    }
    /* send file */
    if (writenbytes(sock2, filedata, datalen) < 0)
    {
      fail_and_exit(sock2, "Failed to send file\n");
    } else {
      minet_close(sock2);
      return 0;
    }

  }
  else // send error response
  {
    if (writenbytes(sock2, (char *)notok_response, strlen(notok_response)) < 0)
    {
        fail_and_exit(sock2, "Failed to write error response\n");
    } else {
      minet_close(sock2);
      return 0;
    }
  }

  /* close socket and free space */
  minet_close(sock2);
  free(filedata);
  return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}

int fail_and_exit(int sock, const char * errorMsg) {
    fprintf(stderr,errorMsg);
    minet_close(sock);
    exit(-1);
}