#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // struct sockaddr_in, htons, htonl
#include <netdb.h> // struct hostent, gethostbyname()
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>


#define SERV_TCP_PORT 40000 // server's "well-known" port number
#define BUF_SIZE 256

void printMenu() {
    printf("\n****************************** MENU ******************************\n");
    printf("pwd - to display current directory of the server\n");
    printf("lpwd - to display current directory of the client\n");
    printf("dir  - to display the file names under the current directory of the server\n");
    printf("ldir - to display the file names under the current directory of the client\n");
    printf("cd diretory_pathname - to change the current directory of the server\n");
    printf("lcd directory_pathname - to change the current directory\n");
    printf("get filename - to download the named file from the current directory of the remote server, and save it in the current directory of the client\n");
    printf("put filename - to upload the named file from the curent directory of the client to the current directory of the remote server\n");
    printf("quit - to terminate the myftp session\n");
    printf("******************************************************************\n\n");
}

int sendOpcode(int sock, char *req) { // To send request (opcode, pathname or filename) to the server
    char msg[BUF_SIZE];
    strcpy(msg, req);

    if (write(sock, msg, BUF_SIZE) == -1) { // Write returns -1 on error
        perror("Error sending request to server.\n");
        return -1; // Request failed to send to server
    }
    return 0; // Request send to server successfully
}

int main(int argc, char *argv[])
{
    // ********** Client initialisation **********
    char host[BUF_SIZE];
    int port;

    // Client running on local host
    gethostname(host, sizeof(host));
    port = SERV_TCP_PORT;

    if (argc == 2) { // Host provided
        strcpy(host, argv[1]); 
    } else if (argc == 3) { // Host and port number provided
        strcpy(host, argv[1]);
        port = atoi(argv[2]);
        while (port < 1 || port > 65535) { // Validation, forcing user to enter valid port number
            printf("Please enter a valid port number (1 - 65535): "); 
            scanf("%d", &port);
        }
    } else if (argc > 3) { // Too many argument
        printf("Please enter up to 3 arguments only (myftp [ hostname | port ]\n");
        exit(1);
    }

    // ****************************************


    // ********** Socket initialisation **********
    int sockfd;
    struct sockaddr_in ser_addr;
    struct hostent *hp;


    // Get host address, & build a server socket address
    bzero((char *) &ser_addr, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(SERV_TCP_PORT);

    if ((hp = gethostbyname(host)) == NULL) {
        printf("Host %s not found\n", host);
        exit(1);
    }
    ser_addr.sin_addr.s_addr = * (u_long *) hp->h_addr;


    // create TCP socket & connect socket to server address
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sockfd, (struct sockaddr *) &ser_addr, sizeof(ser_addr)) < 0) {
        perror("client connect");
        exit(1);
    }

    // ********** User input **********
    char line[BUF_SIZE]; // For user input
    char filename[BUF_SIZE]; // For file name
    char clientReq[BUF_SIZE]; // For client's request to server
    char serverRes[BUF_SIZE]; // For server's response
    char serverOpcode[BUF_SIZE]; // For server's response opcode

    char *inputArray[2]; // To tokenise the input, and store into an array. Element 0 will be the menu, while Element 1 will be either filename or dir_pathname
    int i = 0; // To iterate through the token array


    while(1) { // Always true loop, to loop through user's input
        printMenu();
        printf("$ ");
        fgets(line, sizeof(line), stdin);

        i = 0; // To reset i value

        strcpy(line, strtok(line, "\n")); // Tokenise the input at the delimiter (\n), and store it back into input
        inputArray[i] = strtok(line, " "); // Tokenise the input at the space, and store it at the first element of the array
        while (inputArray[i] != NULL) {
            inputArray[++i] = strtok(NULL, " ");
        }

        strcpy(clientReq, inputArray[0]); // Set client request to be the first element of the input array

        // ********** pwd Request **********
        if (strcmp(clientReq, "pwd") == 0) {
            sendOpcode(sockfd, "P"); // Sending opcode 'P' to server for pwd request
            
            if (read(sockfd, serverOpcode, sizeof(serverOpcode)) == -1) {
                printf("Error, unable to receive response from server\n");
            } else {
                if (strcmp(serverOpcode, "P") == 0) {
                    printf("Successfully received a response from server\n");
                    read(sockfd, serverRes, sizeof(serverRes));
                    printf("Current directory of server: %s\n", serverRes);
                } else {
                    printf("Invalid response\n");
                }
            }

        // ********** lpwd Request **********
        } else if (strcmp(clientReq, "lpwd") == 0) {
            char currentDir[BUF_SIZE];
            getcwd(currentDir, sizeof(currentDir)); // Get current working directory
            printf("Current directory of client: %s\n", currentDir);


        // ********** dir Request **********
        } else if (strcmp(clientReq, "dir") == 0) {
            //printf("Sending opcode 'D' to server for dir request\n");
            sendOpcode(sockfd, "D");

            if (read(sockfd, serverOpcode, sizeof(serverOpcode)) == -1) {
                printf("Error, unable to receive response from server\n");
            }else {
                if (strcmp(serverOpcode, "D") == 0) {
                    printf("Successfully received a response from server\n");
                    read(sockfd, serverRes, sizeof(serverRes));
                    printf("Files in server directory:\n%s\n", serverRes);
                } else if (strcmp(serverOpcode, "X") == 0) {
                    printf("Error from the server\n");
                } else {
                    printf("Invalid response\n");
                }
            }

        // ********** ldir Request **********
        } else if (strcmp(clientReq, "ldir") == 0) {
            struct dirent *dirEntry;
            char currentDir[BUF_SIZE];
            getcwd(currentDir, sizeof(currentDir)); // Get current working directory
            DIR *dir;

            dir = opendir(currentDir);
            if (dir == NULL) {
                printf("Unable to open folder:\n");
            } else {
                printf("Files in %s\n", currentDir);
                while ((dirEntry = readdir(dir))) {
                    if (dirEntry->d_name[0] != '.') {
                        printf("%s\n", dirEntry->d_name);
                    }
                }
            }

        // ********** cd Request **********
        } else if (strcmp(clientReq, "cd") == 0) {

            if (i < 2) { // No arguments
                printf("Please enter a filename\n");
            } else {
                sendOpcode(sockfd, "C"); // Sending opcode 'C' to server for cd request
                strcpy(filename, inputArray[1]);

                if (write(sockfd, filename, sizeof(filename)) == -1) {
                    printf("Failed to send directory to server\n");
                } else {
                    read(sockfd, serverRes, sizeof(serverRes));

                    if (strcmp(serverRes, "C") == 0) {
                        read(sockfd, serverRes, sizeof(serverRes));
                        printf("Current directory: %s\n", serverRes);
                    } else if (strcmp(serverRes, "E") == 0) {
                        printf("Failed to change directory\n");
                    } else if (strcmp(serverRes, "X") == 0) {
                        printf("Error from the server\n");
                    } else {
                        printf("Invalid response\n");
                    }
                }
            }

        // ********** lcd Request **********
        } else if (strcmp(clientReq, "lcd") == 0) {
            if (i < 2) { // No arguments
                printf("Please enter a filename\n");
            } else {
                if (chdir(inputArray[1]) == -1) {
                    printf("Failed to change directory\n");
                } else {
                    printf("Change directory to %s\n", inputArray[1]);
                    char currentDir[BUF_SIZE];
                    getcwd(currentDir, sizeof(currentDir)); // Get current working directory
                    printf("Current directory: %s\n", currentDir);
                }
            }

        // ********** get Request **********
        } else if (strcmp(clientReq, "get") == 0) {
            if (i < 2) { // No arguments
                printf("Please enter a filename\n");
            } else {
                strcpy(filename, inputArray[1]);
                sendOpcode(sockfd, "G"); // Sending opcode 'G' to server for get request
                
                write(sockfd, filename, sizeof(filename)); // Sending filename to the server

                read(sockfd, serverOpcode, sizeof(serverOpcode));
                if (strcmp(serverOpcode, "G") == 0) {
                    // Download the file
                    int fd = open(filename, O_WRONLY | O_CREAT, 0666);
                    if (fd == -1) {
                        printf("Unable to open file\n");
                    } else {
                        printf("Downloading file\n");
                        char buf[BUF_SIZE];
                        int bytes_read = read(sockfd, buf, sizeof(buf));
                        while (1) {
                            void *bufPtr = buf;
                            while (bytes_read > 0) {
                                int bytes_written;
                                if ((bytes_written = write(fd, buf, bytes_read)) <= 0) {
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
                        printf("File %s downloaded\n", filename);
                    }
                } else if (strcmp(serverOpcode, "X") == 0) {
                    printf("Error from the server\n");
                } else if (strcmp(serverOpcode, "N") == 0 ) {
                    printf("File does not exist\n");
                } else {
                    printf("Invalid response\n");
                }
            }

        // ********** put Request **********
        } else if (strcmp(clientReq, "put") == 0) {
            if (i < 2) { // No arguments
                printf("Please enter a filename\n");
            } else {
                sendOpcode(sockfd, "U"); // Sending opcode 'U' to server for put request
                strcpy(filename, inputArray[1]);
                write(sockfd, filename, sizeof(filename)); // Sending filename to the server
                read(sockfd, serverOpcode, sizeof(serverOpcode));

                if (strcmp(serverOpcode, "U") == 0) {
                    int fd;
                    fd = open(filename, O_RDONLY);

                    if (fd == -1) {
                        printf("File does not exist\n");
                    } else {
                        char buf[BUF_SIZE];
                        struct stat fileInfo;
                        fstat(fd, &fileInfo);

                        int bytes_read = read(fd, buf, sizeof(buf));

                        while(1) {
                            void *bufPtr = buf;
                            while (bytes_read > 0) {
                                int bytes_written;
                                if ((bytes_written = write(sockfd, buf, bytes_read)) <= 0) {
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
                        printf("File uploaded to server\n");
                    }
                } else if (strcmp(serverOpcode, "N") == 0) {
                    printf("Server unable to download file\n");
                } else {
                    printf("Invalid response\n");
                }
            }

        // ********** quit Request **********
        } else if (strcmp(clientReq, "quit") == 0) {
            printf("Exiting client ...\n");
            exit(0);

        // ********** Invalid Request **********
        } else {
            printf("Invalid Request\n");
        }
    }
    return 0;
}
