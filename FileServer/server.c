#include <fnmatch.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>

//#define BACKLOG (10)
#define BACKLOG (0)
#define SERV_SIZE 1024
#define RESPONSE_SIZE 4098
#define MAX_STR_SIZE 257
#define DIR_LIST 512

struct thread_arg {
	int clientSocket;
	char rootFolder[MAX_STR_SIZE]; 
};


/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */
char* parseRequest(char* request) {
  //assume file paths are no more than 256 bytes + 1 for null. 
  char *buffer = malloc(sizeof(char)*MAX_STR_SIZE);
  memset(buffer, 0, MAX_STR_SIZE);
  
  if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0; 

  sscanf(request, "GET %s HTTP/1.", buffer);
  return buffer; 
}


void getRequest(void* ptr){
  int socket;
  char buf[SERV_SIZE];
  char completePath[MAX_STR_SIZE];
  char fileName[MAX_STR_SIZE];
  char directoryPath[MAX_STR_SIZE];
  int recvBytes; 
  struct stat currFile;
  struct thread_arg *argPtr = (struct thread_arg *)ptr;

    memset(&buf, 0, SERV_SIZE);
    socket = argPtr->clientSocket;
  
    recvBytes = recv(socket, buf, SERV_SIZE, 0);
    if(recvBytes < 0) { 
        perror("Receive failed");  
        exit(0); 
		}   
    if(recvBytes == 0){
        printf("Client closed connection"); 
        return;
		}
    
    // Parse the request to obtain the directoryPath requested    
    // printf("sscanf returned %d\n",sscanf(buf,"GET %s ",directoryPath));
    sscanf(buf,"GET %s ", directoryPath);
    // printf("Requested directoryPath is %s\n", directoryPath); // ADD_BACK
    
    sprintf(completePath,"%s%s", argPtr->rootFolder, directoryPath);
    printf("Complete directoryPath: %s\n", completePath);    
    
    //check to see if it was a directory or file requested
    if( stat(completePath, &currFile) == -1 ){
        perror("ERROR: Obtaining file status ");
        
        // Display Page Not Found
        char *pageNotFound = "HTTP/1.0 404 Not Found\r\n\r\n<html><h1>Page Not Found!</h1></html>";
        // Checks to make sure error message was sent successfully
        if( send(socket, pageNotFound, strlen(pageNotFound), 0) != strlen(pageNotFound) ){ 
            perror("ERROR: Failed to send: "); 
            exit(0); 
        } 
        // Shutdown current connection and close current socket
        shutdown(socket, SHUT_RDWR);
        close(socket);    
        return;    
    }   
    
    // =========================================
    // If the path requested by the client is a directory, we handle the request
    // as if it's for the index.html
    if((currFile.st_mode & S_IFMT) == S_IFDIR) {   
        DIR * dirPtr = opendir(completePath);
        struct dirent *dp;
    
        // Check if index.html is in the current directory
        // Store all file names in a 2-D array
        int i = 0;
        int flag = 0;
        char directoryList[DIR_LIST][DIR_LIST];
        while( (dp = readdir(dirPtr)) != NULL ){
          strcpy(directoryList[i], dp->d_name);
          if ( strcmp("index.html", dp->d_name) == 0){
            flag = 1; // index.html exists
            break;
          }
          i++; 
        }
        directoryList[i][0] = '\0'; // Marks end of list
        // If index.html does not exist in the current directory,
        // then list the contents of the directory instead
        if(flag == 0){
            char htmlRequestList[1024];
            char responseHeader[1024];
            char *validResponse = "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: ";
            char* tp = "text/html";
            sprintf( htmlRequestList, "%s%s\r\n\r\n", validResponse, tp);
            strcat(htmlRequestList, "<html><body>Links: ");

            i = 0;

            while(directoryList[i][0] != '\0'){
                strcat(htmlRequestList ,"<a href=\"");
                strcat(htmlRequestList, directoryList[i]);
                strcat(htmlRequestList, "\">");
                strcat(htmlRequestList, directoryList[i]);
                strcat(htmlRequestList, "  ");
                strcat(htmlRequestList, "</a>");
                i++;
            }
            strcat(htmlRequestList, "</body></html>"); // Completes the HTML code
            
            send(socket, htmlRequestList, strlen(htmlRequestList), 0);
            shutdown(socket,SHUT_RDWR);
            close(socket);
            return;
        }
        /*
<body> This is a test text. <a href="pic.html">test link</a></body></html>

<html><body>
<img src="monorail.jpg">
<img src="skype.png">
<img src="google.gif">
</body></html>

        */

        // We server the index.html file
        // It is assumed that requests will always end with '/', so this is extra.
        if( completePath[strlen(completePath)-1] != '/')    
            sprintf(fileName, "%s%s", completePath, "/index.html");     // Include a '/' if it's a sub-directory
        else
            sprintf(fileName, "%s%s", completePath, "index.html");
    
        printf("The directory filename is %s\n", fileName);
        // Serve the file content
        char httpHeader[1024];
  
        // Construct the response header
        char *validResponse = "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: ";
        sprintf( httpHeader, "%s%s\r\n\r\n", validResponse, "text/html");
        // Print the http response header to stdout
        printf("HTTP RESPONSE HEADER\n%s", httpHeader);
  
        // Read the file    
        FILE *filePtr = fopen(fileName, "r");
        if(filePtr != NULL){
            // Send the response header first
            if( send(socket, httpHeader, strlen(httpHeader), 0) != strlen(httpHeader)){
			 		      perror("ERROR: Failed to send "); 
                exit(0); 
            }
            char buffer[RESPONSE_SIZE];
            memset(buffer, 0, RESPONSE_SIZE);
            int fd = fileno(filePtr); // Gets file descriptor of filePtr

            int bytes = read(fd, buffer, RESPONSE_SIZE);
            while(bytes != 0){ // Loop until complete request has been sent
              // Send each charcter as you read        
              if( send(socket, buffer, bytes, 0) != bytes ){ 
			 		        perror("ERROR: Failed to send "); 
                  exit(0); 
			 		    }
              bytes = read(fd, buffer, RESPONSE_SIZE);
            }
            fclose(filePtr);
        }
        else{
         	  perror("ERROR: Opening file failed: ");
            char *pageNotFound = "HTTP/1.0 404 Not Found\r\n\r\n<html><h1>Page Not Found!</h1></html>";
            if( send(socket, pageNotFound, strlen(pageNotFound), 0) != strlen(pageNotFound) ){
                perror("ERROR: Failed to send: "); 
                exit(0); 
            }
            shutdown(socket,SHUT_RDWR);
            close(socket);
            return;
        }

        free(argPtr);
        shutdown(socket,SHUT_RDWR);
        close(socket); 
    } 
    // =========================================
    else if( (currFile.st_mode & S_IFMT) == S_IFREG){
        printf("regular file\n");
        //we serve the appropriate file
        //serve the file content
        char httpHeader[1024];
        char extension[MAX_STR_SIZE];
        char fileType[MAX_STR_SIZE];
  
        //Extract the extension  
        int idx = 0;
        int len = strlen(directoryPath);
        //while( (idx < strlen(directoryPath)) && (directoryPath[idx ]  != '.') )
        while( (idx   < len) && (directoryPath[idx ]  != '.') )
            idx++; // Traverse to file extension
        strcpy( extension, &directoryPath[idx + 1]);
        printf("Extension: %s\n", extension);
        //printf("The first part is %s\n",firstPart);
  
        // Find the media type
        if(strcmp(extension,"html")==0) 
				  	sprintf(fileType,"%s","text/html");
        else if(strcmp(extension,"ico")==0) 
				  	sprintf(fileType,"%s","image/vnd.microsoft.icon");
        else if(strcmp(extension,"gif")==0) 
				  	sprintf(fileType,"%s","image/gif");
        else if(strcmp(extension,"jpg")==0) 
				  	sprintf(fileType,"%s","image/jpeg");
        else if(strcmp(extension,"pdf")==0) 
				  	sprintf(fileType,"%s","application/pdf");
        else if(strcmp(extension,"png")==0) 
				  	sprintf(fileType,"%s","image/png");
        else if(strcmp(extension,"txt")==0) 
            sprintf(fileType,"%s","text/plain");
        // Construct the http response header
        char *validResponse = "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: ";
  
        sprintf( httpHeader, "%s%s\r\n\r\n", validResponse, fileType);
  
        //print the http response header
        printf("HTTP RESPONSE HEADER\n%s", httpHeader);
  
        //read the file    
        FILE *filePtr = fopen(completePath,"r");
        if(filePtr != NULL){
            //send the response header first
            if( send(socket, httpHeader, strlen(httpHeader),0) != strlen(httpHeader)){
				    		perror("ERROR: Failed to send "); 
				    		exit(0); 
				    }
            char buffer[RESPONSE_SIZE];
            memset(buffer, 0, RESPONSE_SIZE);
            int fd = fileno(filePtr);
            int bytes = read(fd, buffer, RESPONSE_SIZE);
            while(bytes != 0){ // Loop until complete request has been sent
              //send each charcter as you read        
              if( send(socket, buffer, bytes, 0) != bytes){
				    			perror("ERROR: Failed to send "); 
				    			exit(0); 
				    	}
              bytes = read(fd, buffer, RESPONSE_SIZE);
            }
            fclose(filePtr);
        }
        else{
            perror("ERROR: Opening file failed: ");
            char *pageNotFound = "HTTP/1.0 404 Not Found\r\n\r\n<html><h1>Page Not Found!</h1></html>";
         	  if(send(socket, pageNotFound, strlen(pageNotFound), 0) != strlen(pageNotFound)){
				    		perror("ERROR: Failed to send: "); 
				    		exit(0); 
				    }
         	  shutdown(socket,SHUT_RDWR);
         	  close(socket);
         	  return;
        }

        free(argPtr);
        shutdown(socket, SHUT_RDWR);
        close(socket);
    } 
}


/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
    /* For checking return values. */
    int retval;
    struct sockaddr_in remote_addr;
    unsigned int socklen = sizeof(remote_addr); 

    // Thread
    struct thread_arg *ptr;
    pthread_t *pThread;
		
    if(argc < 3){
			printf("Error: missing arguments\n");
			exit(0);
		}

    /* Read the port number from the first command line argument. */
    int port = atoi(argv[1]);

    /* Create a socket to which clients will connect. */
    int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("Creating socket failed");
        exit(0);
    }

    /* A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options. */
    int reuse_true = 1;
    retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));
    if (retval < 0) {
        perror("Setting socket option failed");
        exit(0);
    }

    /* Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here. */
    
   
    struct sockaddr_in6 addr;   // internet socket address data structure
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port); // byte order is significant
    addr.sin6_addr = in6addr_any; // listen to all interfaces

    
    /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
    retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if(retval < 0) {
        perror("Error binding to port");
        exit(0);
    }

		
    /* Another address structure.  This time, the system will automatically
     * fill it in, when we accept a connection, to tell us where the
     * connection came from. */
    

    while(1) {
    /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
    retval = listen(server_sock, BACKLOG);
    if(retval < 0) {
        perror("Error listening for connections");
        exit(0);
    }

        /* Declare a socket for the client connection. */
        int sock;
        char buffer[256];

        /* Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         * */
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        if(sock < 0) {
            perror("Error accepting connection");
            exit(0);
        }

				ptr = (struct thread_arg*)malloc(sizeof(struct thread_arg));
				pThread = (pthread_t*)malloc(sizeof(pthread_t));
				ptr->clientSocket = sock;
				strcpy(ptr->rootFolder, argv[2]);
				pthread_create (pThread, NULL, (void *) &getRequest, (void *) ptr);

        /* At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). */

        /* ALWAYS check the return value of send().  Also, don't hardcode
         * values.  This is just an example.  Do as I say, not as I do, etc. */
        //send(sock, "Hello, client.\n", 15, 0);
        //recv(sock,buffer,MAX_STR_SIZE,0);

        /* Tell the OS to clean up the resources associated with that client
         * connection, now that we're done with it. */
        //close(sock);
    }

    //close(server_sock);
}


