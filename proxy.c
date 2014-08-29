/* this is not a working program yet, but should help you get started */

#include <stdio.h>
#include "csapp.h"
#include "proxy.h"
#include <pthread.h>

#define   LOG_FILE      "proxy.log"
#define   DEBUG_FILE	"proxy.debug"

/*============================================================
 * function declarations
 *============================================================*/

int  find_target_address(char * uri,
			 char * target_address,
			 char * path,
			 int  * port);


void  format_log_entry(char * logstring,
		       int sock,
		       char * uri,
		       int size);
		       
void *webTalk(void* args);
void ignore();
int debug;
int proxyPort;
int debugfd;
int logfd;
pthread_mutex_t mutex;

/*if (numBytes == 0){
  break;
  }*/
/* main function for the proxy program */

int main(int argc, char *argv[])
{

  int count = 0;
  int listenfd, connfd, clientlen, optval, serverPort, i;
  struct sockaddr_in clientaddr;
  struct hostent *hp;
  char *haddrp;
  sigset_t sig_pipe; 
  pthread_t tid;
  //int *args;
  if (argc < 2) {
    printf("Usage: ./%s port [debug] [serverport]\n", argv[0]);
    exit(1);
  }
  proxyPort = atoi(argv[1]);

  /* turn on debugging if user enters a 1 for the debug argument */

  if(argc > 2)
    debug = atoi(argv[2]);
  else
    debug = 0;

  if(argc == 4)
    serverPort = atoi(argv[3]);
  else
    serverPort = 80;

  /* deal with SIGPIPE */
  /*  Signal(SIGPIPE, ignore);
  printf("sigpipe done");
  if(sigemptyset(&sig_pipe) || sigaddset(&sig_pipe, SIGPIPE))
    unix_error("creating sig_pipe set failed");

  if(sigprocmask(SIG_BLOCK, &sig_pipe, NULL) == -1)
    unix_error("sigprocmask failed");
  */
  /* important to use SO_REUSEADDR or can't restart proxy quickly */


  listenfd = Open_listenfd(proxyPort);
  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); 
  
  if(debug) debugfd = Open(DEBUG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);

  logfd = Open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);    
  
  /* protect log file with a mutex */

  pthread_mutex_init(&mutex, NULL);
  

  /* not wait for new requests from browsers */

  while(1) {
    //    printf("a\n");  
    clientlen = sizeof(clientaddr);

    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    
    hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
		       sizeof(clientaddr.sin_addr.s_addr), AF_INET);

    haddrp = inet_ntoa(clientaddr.sin_addr);
     
    int *args=malloc(2*sizeof(int));
    
    args[0] = connfd; args[1] = serverPort;

    /* spawn a thread to process the new connection */

    Pthread_create(&tid, NULL, webTalk, (void*) args);
    Pthread_detach(tid);
  }


  /* should never get here, but if we do, clean up */

  Close(logfd);  
  if(debug) Close(debugfd);

  pthread_mutex_destroy(&mutex);
  
}

void parseAddress(char* url, char* host, char** file, int* serverPort)
{
	char* point1, *point2;
	char *saveptr = NULL;
	printf("url: %s\n", url);
	if(strstr(url, "http://"))
		url = &(url[7]);
	printf("1\n");
	*file = strchr(url, '/');
	printf("2\n");
	strcpy(host, url);
	printf("3\n");
	point1 = strchr(url, ':');
	printf("4\n");
	strtok_r(host, ":/",&saveptr);  //should we replace it? become worse
	printf("5\n");
	if(!point1) {
		*serverPort = 80;
		return;
	}
	printf("6\n");
	*serverPort = atoi(strtok_r(NULL, ":/",&saveptr));
}

/*void secureTalk(int &clientfd, rio_t &client, char *host, char *version, const int serverport)

*/

void *serverTalk(void *args) {
  char buf[MAXLINE];  
  ssize_t numBytes =0;
  int serverfd = ((int*)args)[1];
  int clientfd = ((int*)args)[0];
  //free(args);
  
  printf("into serverTalk\n");

  while (1){
    numBytes = Rio_readp(serverfd,buf,MAXLINE);
    printf("buf in server talk:%s\n",buf);
    if (numBytes == 0){
      break;
    }
    Rio_writen(clientfd,buf,numBytes);
  }
  // free(args);
  return NULL;
}

void *clientTalk(void *args) {
  ssize_t numBytes =0;
  char buf[MAXLINE];
  int clientfd = ((int*)args)[0];
  int serverfd = ((int*)args)[1];
  //free(args);
  
  printf("into client talk\n");

  while(1){
    numBytes = Rio_readp(clientfd,buf,MAXLINE);
    if (numBytes == 0){
      break;
    }    
    printf("buf in client talk:%s\n",buf);
    Rio_writen(serverfd,buf,numBytes);
  }
  // free(args);
  return NULL;
}

void secureTalk(int clientfd, rio_t client, char *host, char *version, const int serverPort){
  ssize_t numBytes;
  int serverfd = Open_clientfd(host,serverPort);
  char buf[MAXLINE];
  bzero(buf,MAXLINE);
  int* fds;
  fds = malloc(2*sizeof(int));
  fds[0] = clientfd; 
  fds[1] = serverfd;
  pthread_t tids, tidc;
  
  const char * temp ="HTTP/1.1 200 Connection established\r\n\r\n";
  strcpy(buf,temp);
  printf("buf:\n%s",buf);


  Rio_writep(clientfd,buf,strlen(buf)); 
  Pthread_create(&tidc, NULL, (void *)&clientTalk, (void*) fds);
  Pthread_create(&tids, NULL, (void *)&serverTalk, (void*) fds); 
  //printf("threads created\n");
 
  Pthread_join(tidc,NULL);
  Pthread_join(tids,NULL);
  //printf("threads detached\n");
  free(fds);
  return;

}




/* WebTalk()
 *
 * Once a connection has been established, webTalk handles
 * the communication.
 */


/* this function is not complete */
/* you'll do the bulk of your work here */

void *webTalk(void* args)
{
  
  int  lineNum, serverfd, clientfd, serverPort;
  ssize_t numBytes;
  int tries;
  int byteCount = 0;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE],buf4[MAXLINE];
  char host[MAXLINE];
  char url[MAXLINE], logString[MAXLINE];
  char *token, *cmd, *version, *file;
  rio_t server, client;
  char slash[10];
  strcpy(slash, "/");
  
  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];
  free(args);

  Rio_readinitb(&client, clientfd);
  
  /* Determine whether request is GET or CONNECT */

  numBytes = Rio_readlineb(&client, buf1, MAXLINE);
  if(strcmp(buf1,"\0") == 0) {
    //unix_error("No data in buf1\n");
    return NULL;
  }
  char*p = NULL;
  strcpy(buf2,buf1);  //copy the first line in buf2
  printf("buf2:%s",buf2);
  cmd = strtok_r(buf1, " \r\n", &p);
  strcpy(url, strtok_r(NULL, " \r\n", &p));
  printf("before parse\n");
  parseAddress(url, host, &file, &serverPort);
  printf("after parse\n"); 
  if(!file) file = slash;
  if(debug) 
    {	sprintf(buf3, "%s %s %i\n", host, file, serverPort); 
      Write(debugfd, buf3, strlen(buf3));}
  
  if(!strcmp(cmd, "CONNECT")) {
    secureTalk(clientfd, client, host, version, serverPort);
    return NULL; }
  else if(strcmp(cmd, "GET")) {
    if (debug) printf("%s",cmd);
    app_error("Not GET or CONNECT");
    return NULL;
  }
  printf("hfs: %s %s %d %s", host, file, serverPort, cmd); 
  serverfd = Open_clientfd(host,serverPort);
  Rio_writen(serverfd,buf2,strlen(buf2));
  printf("serverfd written, buf2:%s\n",buf2);


 /* int temp = 0;
  for (; temp != 8; ++temp){
    Rio_readlineb(&client,buf4,MAXLINE);
    strcat(buf2,buf4); 
  }
  printf("%s\n",buf2);*/
  
  while(1){
    numBytes = Rio_readlineb(&client,buf2,MAXLINE);
    
    Rio_writen(serverfd,buf2,numBytes);
    if (strcmp(buf2,"\r\n") == 0){   //no data to be read
      break;
    }
  }
  printf("buf2:%s",buf2);
  printf("send complete\n");
  
 // memset(buf2,0,MAXLINE);
  //memset(buf4,0,MAXLINE);

  while(1){
   numBytes = Rio_readp(serverfd,buf2,MAXLINE);
   printf("receive from server(buf2):\n%s",buf2);
   if (numBytes == 0){   //no data to be read
       break;
   }
   Rio_writen(clientfd,buf2,numBytes);
  }
  printf("Get completed!\n");


  //  printf("url:%s\nhost:%s\nfile:%s\nversion:%s\nserverport:%d\n",url,host,file,vers,serverPort);
  //  printf("buf2:%s\n",buf2);
  //  printf("buf3:%s\n",buf3);
  

  /* you should insert your code for processing connections here */

   
  
  /* code below writes a log entry at the end of processing the connection */

  pthread_mutex_lock(&mutex);
  
  format_log_entry(logString, serverfd, url, byteCount);
  Write(logfd, logString, strlen(logString));
  
  pthread_mutex_unlock(&mutex);
  
  /* 
     When EOF is detected while reading from the server socket,
     send EOF to the client socket by calling shutdown(clientfd,1);
     (and vice versa) 
  */	
  
  Close(clientfd);
  Close(serverfd);
  return NULL;
}


void ignore()
{
	;
}


/*============================================================
 * url parser:
 *    find_target_address()
 *        Given a url, copy the target web server address to
 *        target_address and the following path to path.
 *        target_address and path have to be allocated before they 
 *        are passed in and should be long enough (use MAXLINE to be 
 *        safe)
 *
 *        Return the port number. 0 is returned if there is
 *        any error in parsing the url.
 *
 *============================================================*/

/*find_target_address - find the host name from the uri */
int  find_target_address(char * uri, char * target_address, char * path,
                         int  * port)

{
 

    if (strncasecmp(uri, "http://", 7) == 0) {
	char * hostbegin, * hostend, *pathbegin;
	int    len;
       
	/* find the target address */
	hostbegin = uri+7;
	hostend = strpbrk(hostbegin, " :/\r\n");
	if (hostend == NULL){
	  hostend = hostbegin + strlen(hostbegin);
	}
	
	len = hostend - hostbegin;

	strncpy(target_address, hostbegin, len);
	target_address[len] = '\0';

	/* find the port number */
	if (*hostend == ':')   *port = atoi(hostend+1);

	/* find the path */

	pathbegin = strchr(hostbegin, '/');

	if (pathbegin == NULL) {
	  path[0] = '\0';
	  
	}
	else {
	  pathbegin++;	
	  strcpy(path, pathbegin);
	}
	return 0;
    }
    target_address[0] = '\0';
    return -1;
}



/*============================================================
 * log utility
 *    format_log_entry
 *       Copy the formatted log entry to logstring
 *============================================================*/

void format_log_entry(char * logstring, int sock, char * uri, int size)
{
    time_t  now;
    char    buffer[MAXLINE];
    struct  sockaddr_in addr;
    unsigned  long  host;
    unsigned  char a, b, c, d;
    socklen_t    len = sizeof(addr);

    now = time(NULL);
    strftime(buffer, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (getpeername(sock, (struct sockaddr *) & addr, &len)) {
	unix_error("Can't get peer name");
    }

    host = ntohl(addr.sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", buffer, a,b,c,d, uri, size);
}
