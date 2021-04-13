#include <unistd.h>
#include <sys/stat.h>
#include <string.h> // strlen(), strcmp() etc
#include <errno.h> // extern int errno, EINTR, perror()
#include <signal.h> // SIGCHLD, sigaction()
#include <syslog.h>
#include <sys/types.h> // pid_t, u_long, u_short
#include <sys/socket.h> // struct sockaddr, socket(), etc
#include <sys/wait.h> // waitpid(), WNOHAND
#include <netinet/in.h> // struct sockaddr_in, htons(), htonl()
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>

// and INADDR_ANY
#define SERV_TCP_PORT 40000 // server port no
#define BUF_SIZE 256

int logfile(char *filename, char *line) {
    FILE *file = fopen(filename, "a"); // Append file
    struct tm *sTm;

    char buf[20];
    time_t now = time(0);
    sTm = gmtime(&now);
    strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", sTm);

    fprintf(file, "%s %s", buf, line);
    fclose(file);

    return 0;
}

int sendOpcode(int sock, char *req) { // To send response (opcode, pathname or filename) to the client
    char msg[BUF_SIZE];
    strcpy(msg, req);

    if (write(sock, msg, BUF_SIZE) == -1) { // Write returns -1 on error
        perror("Error in sending request to client.\n");
        return -1; // Response failed to send to client
    }
    return 0; // Response send to client successfully
}

void claim_children()
{
    pid_t pid=1;

    while (pid>0)   // claim as many zombies as we can
    {
        pid = waitpid(0, (int *)0, WNOHANG);
    }
}
void daemon_init(void)
{       
     pid_t   pid;
     struct sigaction act;

     if ( (pid = fork()) < 0) {
          perror("fork"); exit(1); 
     } else if (pid > 0) {
          printf("PID: %d\n", pid);
          logfile("log.txt", "Server started\n");
          exit(0);
     }

     /* child continues */
     setsid();                      /* become session leader */
     chdir("/");                    /* change working directory */
     umask(0);                      /* clear file mode creation mask */

     /* catch SIGCHLD to remove zombies from system */
     act.sa_handler = claim_children; /* use reliable signal */
     sigemptyset(&act.sa_mask);       /* not to block other signals */
     act.sa_flags   = SA_NOCLDSTOP;   /* not catch stopped children */
     sigaction(SIGCHLD,(struct sigaction *)&act,(struct sigaction *)0);
     /* note: a less than perfect method is to use 
              signal(SIGCHLD, claim_children);
     */
}

void serve_a_client(int sd) {
    char clientOpcode[BUF_SIZE];
    char clientReq[BUF_SIZE];
    char serverRes[BUF_SIZE];

    // read a message from client
    while (1) {
        if (read(sd, clientOpcode, sizeof(clientOpcode)) <= 0) {
            return; // connection broken down
        }

        // ********** pwd Response **********
        if (strcmp(clientOpcode, "P") == 0) {
            // logfile("log.txt", "Received opcode 'P' (pwd)\n");

            char currentDir[BUF_SIZE];
            getcwd(currentDir, sizeof(currentDir));
            strcpy(serverRes, currentDir); 

            sendOpcode(sd, "P");
            if (write(sd, serverRes, sizeof(serverRes)) == -1) {
                // logfile("log.txt", "Error sending response to client\n");
            } else {
                // logfile("log.txt", "Successfully send response back to client\n");
            }
        
        // ********** dir Response **********
        } else if (strcmp(clientOpcode, "D") == 0) {
            // logfile("log.txt", "Received opcode 'D' (dir)\n");

            struct dirent *dirEntry;
            char currentDir[BUF_SIZE];
            getcwd(currentDir, sizeof(currentDir)); // Get current working directory
            DIR *dir;

            dir = opendir(currentDir);
            if (dir == NULL) {
                // logfile("log.txt", "Sending opcode 'X' to client\n");
                sendOpcode(sd, "X");
                // logfile("log.txt", "Unable to open folder\n");
            } else {
                // logfile("log.txt", "Sending opcode 'D' to client\n");
                sendOpcode(sd, "D");
                char msg[BUF_SIZE] = "";
                while ((dirEntry = readdir(dir))) {
                    if (dirEntry->d_name[0] != '.') {
                        strcat(msg, dirEntry->d_name);
                        strcat(msg, "\n");
                    }
                }
                strcpy(serverRes, msg);
            }

            
            if (write(sd, serverRes, sizeof(serverRes)) == -1) {
                // logfile("log.txt", "Error in sending response back to client\n");
            } else {
                // logfile("log.txt", "Successfully send response back to client\n");
            }

        // ********** cd Response **********
        } else if (strcmp(clientOpcode, "C") == 0) {
            /// logfile("log.txt", "Received opcode 'C' (cd)");

            if (read(sd, clientReq, sizeof(clientReq)) == -1) {
                // logfile("log.txt", "Error in reading request (cd)");
                sendOpcode(sd, "X");
            } else {
                if (chdir(clientReq) == -1) {
                    sendOpcode(sd, "E");
                } else {
                    sendOpcode(sd, "C");
                    char currentDir[BUF_SIZE];
                    getcwd(currentDir, sizeof(currentDir));
                    write(sd, currentDir, sizeof(currentDir));
                }
            }

        // ********** get Response **********
        } else if (strcmp(clientOpcode, "G") == 0) {
            // logfile("log.txt", "Received opcode 'G' (get)\n");
            if (read(sd, clientReq, sizeof(clientReq)) == -1) {
                // logfile("log.txt", "Error in reading request (get)\n");
                sendOpcode(sd, "X");
            } else {
                int fd;
                char fileName[BUF_SIZE];
                strcpy(fileName, clientReq);
                fd = open(fileName, O_RDONLY);

                if (fd == -1) { // File do not exist
                    // logfile("log.txt", "Error, no such file (get)\n");
                    sendOpcode(sd, "N");
                } else {
                    // logfile("log.txt", "File opened (get)\n");
                    sendOpcode(sd, "G");
                    char buf[BUF_SIZE];
                    struct stat fileInfo;
                    fstat(fd, &fileInfo);

                    int bytes_read = read(fd, buf, sizeof(buf));

                    while(1) {
                        void *bufPtr = buf;
                        while (bytes_read > 0) {
                            int bytes_written;
                            if ((bytes_written = write(sd, buf, bytes_read)) <= 0) {
                                break;
                            }
                            bytes_read -= bytes_written;
                            bufPtr += bytes_written;
                        }
                        if ((bytes_read = read(fd, buf, sizeof(buf))) <= 0) {
                            break;
                        }
                    }
                    close(fd);
                    // logfile("log.txt", "File transferred (GET)\n");
                }
            }

        // ********** put Response **********
        } else if (strcmp(clientOpcode, "U") == 0) {
            // logfile("log.txt", "Received opcode 'U' (put)\n");
            char fileName[BUF_SIZE];
            read(sd, clientReq, sizeof(clientReq));
            strcpy(fileName, clientReq);
            int fd;
            fd = open(fileName, O_WRONLY | O_CREAT, 0666);

            if (fd == -1) {
                // logfile("log.txt", "Unable to open file\n");
                sendOpcode(sd, "N");
            } else {
                // logfile("log.txt", "Downloading file\n");
                sendOpcode(sd, "U");
                char buf[BUF_SIZE];
                int bytes_read = read(sd, buf, sizeof(buf));
                while(1) {
                    void *bufPtr = buf;
                    while (bytes_read > 0) {
                        int bytes_written;
                        if ((bytes_written = write(fd, buf, bytes_read)) > 0) {
                            break;
                        }
                        bytes_read -= bytes_written;
                        bufPtr +=  bytes_written;
                    }
                    if ((bytes_read = read(fd, buf, sizeof(buf))) <= 0) {
                        break;
                    }
                }
                // logfile("log.txt", "File downloaded\n");
                close(fd);
            }
        }
    }
}

int main()
{
    int sd, nsd, n, cli_addrlen;
    pid_t pid;
    struct sockaddr_in ser_addr, cli_addr;
    // turn the process into a daemon
    daemon_init();

    // set up listening socket sd
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("server:socket");
        exit(1);
    }

    // build server listening socket address
    bzero((char *)&ser_addr, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(SERV_TCP_PORT);
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* note: accept client request sent to any one of the
    network interface(s) on this host.
    */

    // bind server address to socket sd
    if (bind(sd, (struct sockaddr *) &ser_addr, sizeof(ser_addr))<0)
    {
        perror("server bind");
        exit(1);
    }

    // become a listening socket
    listen(sd, 5);

    while (1)
    {
        // wait to accept a client request for connection
        cli_addrlen = sizeof(cli_addr);
        nsd = accept(sd, (struct sockaddr *) &cli_addr,
                     (socklen_t *) &cli_addrlen);
        if (nsd < 0)
        {
            if (errno == EINTR) // if interrupted by SIGCHLD
                continue;
            perror("server:accept");
            exit(1);
        }

        // create a child process to serve this client
        if ((pid=fork()) <0)
        {
            perror("fork");
            exit(1);
        }
        else if (pid > 0)
        {
            close(nsd);
            continue; // parent to wait for next client
        }

        // now in child, serve the current client
        close(sd);
        // logfile("log.txt", "Server shutting down\n");
        serve_a_client(nsd);
        exit(0);
    }
}
