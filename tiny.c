/* $begin tinymain */
/*
 * <Mrigank Doshy       911248894>
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"

// type = 0, not a range request
// type = 1, range request of the form r1-r2
// type = 2, range request of the form r1-
// type = 3, range request of the form -r1
typedef struct rangeNode {
  int type;
  int first;
  int second;
} rangeNode;

void doit(int fd);
void read_requesthdrs(rio_t *rp, rangeNode *nodePtr);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int size_flag,
    rangeNode *nodePtr);
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

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN);
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
        port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);                                             //line:netp:tiny:doit
    Close(connfd);                                            //line:netp:tiny:close
  }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  rangeNode range = {0, 0, 0}; 

  /* Read request line and headers */
  rio_readinitb(&rio, fd); 
  if (!rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
    return;
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
  if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
    clienterror(fd, method, "501", "Not Implemented",
        "Tiny does not implement this method");
    return;
  }                                                    //line:netp:doit:endrequesterr
  read_requesthdrs(&rio, &range);                              //line:netp:doit:readrequesthdrs

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);       //line:netp:doit:staticcheck
  if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
    clienterror(fd, filename, "404", "Not found",
        "Tiny couldn't find this file");
    return;
  }                                                    //line:netp:doit:endnotfound

  if (is_static) { /* Serve static content */          
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
      clienterror(fd, filename, "403", "Forbidden",
          "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, is_static, &range);        //line:netp:doit:servestatic
  }
  else { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
      clienterror(fd, filename, "403", "Forbidden",
          "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
  }
}
/* $end doit */

void process_range(char *buf, rangeNode *nodePtr) {
  char *next_tok;
  int r1, r2;
  if ((next_tok = strstr(buf, "bytes=")) != NULL) {
    next_tok = next_tok + 6; 
    if (sscanf(next_tok, "-%u", &r1) == 1) {
      nodePtr->type = 3;
      nodePtr->first = -r1; 
    }
    else if (sscanf(next_tok, "%u-%u",&r1, &r2) == 2) {
      nodePtr->type = 1;
      nodePtr->first = r1;
      nodePtr->second = r2;
    } 
    else if (sscanf(next_tok, "%u-",&r1) == 1) {
      nodePtr->type = 2;
      nodePtr->first = r1; 
    } 
    else {
      nodePtr->type = 0;
      printf("get range: error\n");
    }
  }
  printf("range type: %d, first: %d, second: %d\n", nodePtr->type, nodePtr->first, nodePtr->second);
}
/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp, rangeNode *nodePtr) 
{
  char buf[MAXLINE];

  if (!rio_readlineb(rp, buf, MAXLINE))
    return;
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
    if (!strncasecmp(buf, "Range:", 6)) {
      process_range(buf, nodePtr);
    }
    rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 *             2 if static but no content-length .nosize
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
  char *ptr;
  int ret_val = 0;
  int len = strlen(uri);

  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    if (len > 7 && !strcmp(&uri[len-7], ".nosize")) {
      strncat(filename, uri, len-7);
      ret_val = 2;
    }
    else {
      strcat(filename, uri);
      ret_val = 1;
    }
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return ret_val;
  }
  else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
    ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else 
      strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
    strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
    strcat(filename, uri);                           //line:netp:parseuri:endconvert2
    return 0;
  }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 * size_flag is 1, provide content-length,
 * size_flag is 2, do not provide content-length
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize, int size_flag, 
    rangeNode *nodePtr) 
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  int length = 0;
  int validity = 0;
  size_t writesize;

  /* Send response headers to client */
  get_filetype(filename, filetype);       //line:netp:servestatic:getfiletype
  /* If a request with no specific range is sent, 
 *  set length to the filesize and print appropriately. */
  if (nodePtr->type == 0) {
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    length = filesize;
  }
 /* RANGE TYPE 1: bytes = r1-r2 */
  else if (nodePtr->type == 1) {
    /* 1. Check if range is equal to the entire file. 
 *     2. If it is, return request like no range was specified at all. 
 *     3. Set the length to be the filesize. 
 *  */
    if ((nodePtr->first == 0) && (nodePtr->second == filesize - 1)) {
      sprintf(buf, "HTTP/1.0 200 OK\r\n");  //line:netp:servestatic:beginserve
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      length = filesize;
    }
    /* This is an edge case: 
 *     1. Check if r1 > filesize or if r1 > r2. 
 *     2. If it is, return appropriate error header of Range not Satisfiable.
 *     3. Print the Accepted Range Bytes and the Content Range Bytes. 
 *     4. Set validilty to be invalid and set length to 0. 
 *  */
    else if ((nodePtr->first > filesize)||(nodePtr->first > nodePtr->second)) {
      sprintf(buf, "HTTP/1.1 416 Range Not Satisfiable\r\n"); //line:netp:servestatic:beginserve
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      sprintf(buf, "%sAccept-Ranges: bytes\r\n", buf);
      sprintf(buf, "%sContent-Range: bytes */%d\r\n", buf, filesize);
      length = 0;
      validity = 1;
    }
    /* 1. Check if r2 > filesize. 
 *     2. If it is, set r2 to filesize - 1 and length to filesize - r1
 *     3. Otherwise, print header of Partial Content.
 *     4. Also print Content Range Bytes as r1, r2, filesize. 
 *  */
    else {
      if (nodePtr->second >= filesize) {
        nodePtr->second = filesize - 1;
        length = 1 + nodePtr->second - nodePtr->first;
      }
      else
        length = 1 + nodePtr->second - nodePtr->first;
      sprintf(buf, "HTTP/1.1 206 Partial Content\r\n");   //line:netp:servestatic:beginserve
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      sprintf(buf, "%sAccept-Ranges: bytes\r\n", buf);
      sprintf(buf, "%sContent-Range: bytes %d-%d/%d\r\n", buf, nodePtr->first, 
              nodePtr->second, filesize);
    }
  }
 /*RANGE TYPE 2: bytes = r1- */
  else if (nodePtr->type == 2) {
    /* 1. Check if range is equal to the entire file. 
 *     2. If it is, return request like no range was specified at all. 
 *     3. Set length to be the filesize 
 *  */
    if (nodePtr->first == 0) {
      sprintf(buf, "HTTP/1.0 200 OK\r\n");  //line:netp:servestatic:beginserve
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      length = filesize;
    }
    /* This is an edge case:
 *     1. Check if r1 > filesize. 
 *     2. If it is, return appropriate error header of Range Not Satisfiable.
 *     3. Print the range accepted and the content range.
 *     4. Set validity to be invalid and set length to 0. 
 *  */
    else if (nodePtr->first >= filesize) {
      sprintf(buf, "HTTP/1.1 416 Range Not Satisfiable\r\n");    //line:netp:servestatic:beginserve
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      sprintf(buf, "%sAccept-Ranges: bytes\r\n", buf);
      sprintf(buf, "%sContent-Range: bytes */%d\r\n", buf, filesize);
      validity = 1;
      length = 0;
    }
    /* 1. Otherwise, print header Partial Content.
 *     2. Print Accepted Range Bytes and Content Range Bytes as r1, 
 *        filesize - 1 and filesize
 *     3. Set length as filesize - r1.
 *  */
    else {
      sprintf(buf, "HTTP/1.1 206 Partial Content\r\n");    //line:netp:servestatic:beginserve
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      sprintf(buf, "%sAccept-Ranges: bytes\r\n", buf);
      sprintf(buf, "%sContent-Range: bytes %d-%d/%d\r\n", buf, nodePtr->first, 
              (filesize-1), filesize);                                         
      length = filesize - nodePtr->first; 
    }     
  }
  /* RANGE TYPE 3: bytes = -r1 */
  else if (nodePtr->type == 3) {
    /* 1. Check if length is 0. 
 *     2. If it is, Print appropriate error header of Range Not Satisfiable.
 *     3. Set validity to invalid and set length to 0. 
 *  */
    if (nodePtr->first == 0) {
      sprintf(buf, "HTTP/1.1 416 Range Not Satisfiable\r\n");    //line:netp:servestatic:beginserve
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      sprintf(buf, "%sAccept-Ranges: bytes\r\n", buf);
      sprintf(buf, "%sContent-Range: bytes */%d\r\n", buf, filesize);
      validity = 1;
      length = 0;
    }
    /* Otherwise compute length, r1, r2 and then return partial headers. */
    else {
      /* 1. First check if r1 >= filesize. 
 *       2. If it is, set r1 to 0. 
 *       3. Set r2 to filesize - 1
 *       4. Set length to be the filesize
 *
 *       5. If it is not, set length to be r1
 *       6. Set r1 to be filesize - r1
 *       7. And set r2 to filesize - 1
 *
 *       8. Then print header of Partial Content
 *       9. Also print Acceted Range Bytes and Content Range Bytes of r1, r2 
 *          and filesize. 
 *    */
      if (abs(nodePtr->first) >= filesize) {
        nodePtr->first = 0;
        nodePtr->second = filesize - 1;
        length = filesize;
      }
      else {
        length = abs(nodePtr->first);
        nodePtr->first = filesize - abs(nodePtr->first);
        nodePtr->second = filesize - 1;
      }
      sprintf(buf, "HTTP/1.1 206 Partial Content\r\n");    //line:netp:servestatic:beginserve
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      sprintf(buf, "%sAccept-Ranges: bytes\r\n", buf);
      sprintf(buf, "%sContent-Range: bytes %d-%d/%d\r\n", buf, nodePtr->first, 
              nodePtr->second, filesize);
    }
  }
  /* Now check for validity. */
  if (validity == 0) {
    if (size_flag == 1)
      sprintf(buf, "%sContent-length: %d\r\n", buf, length);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  }
   
  writesize = strlen(buf);
  if (rio_writen(fd, buf, strlen(buf)) < writesize) {
    printf("errors writing to client.\n");       //line:netp:servestatic:endserve
  }
  printf("Response headers:\n");
  printf("%s", buf);
  if (validity == 0) {
    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//line:netp:servestatic:mmap
    Close(srcfd);                           //line:netp:servestatic:close
    if (rio_writen(fd, srcp + nodePtr->first, length) < length) {
      printf("errors writing to client.\n");         //line:netp:servestatic:write
    }
  Munmap(srcp, filesize);                 //line:netp:servestatic:munmap
  }
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else if (strstr(filename, ".mp3"))
    strcpy(filetype, "audio/mp3");
  else
    strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
  rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
    Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
    Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
  }
  Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
    char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  sprintf(buf, "%sContent-type: text/html\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, (int)strlen(body));
  rio_writen(fd, buf, strlen(buf));
  rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
