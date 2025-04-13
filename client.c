#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE (10*1024*1024)  /* 10MB buffer size */
#define MIN_PORT 1024
#define MAX_PORT 65535

/* Global variables for cleanup */
int sockfd = -1;     /* Socket descriptor */
int fd_in = -1;      /* Input file descriptor */
char *buffer = NULL; /* Buffer for file data */

/* Function prototypes */
void cleanup(void);
void SIGINT_handler(int sig);

/* Cleanup function - from handout */
void cleanup(void) {
    /* Free ONLY if it's safe to do so... */
    if(buffer != NULL) {
        free(buffer);         /* Release the memory */
        buffer = NULL;        /* Mark the pointer so that we know we have
                                already released it and avoid a 'dangling
                                pointer. */
    }

    /* Free file descriptor */
    if(fd_in > -1) {
        close(fd_in);         /* Close the file */
        fd_in = -1;           /* Mark it as such */
    }

    /* Free socket descriptor */
    if(sockfd > -1) {
        close(sockfd);        /* Close the socket */
        sockfd = -1;          /* Mark it as such */
    }
}

/* SIGINT handler for the client - from handout */
void SIGINT_handler(int sig) {

   /* Issue an error */
   fprintf(stderr, "client: Client interrupted. Shutting down.\n");

   /* Cleanup after yourself */
   cleanup();

   /* Exit for 'reals' */
   exit(EXIT_FAILURE);

} /* end SIGINT_handler() */

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    int port;
    int i;
    ssize_t bytes_read;

    /* Validate command line arguments */
    if(argc < 3) {
        fprintf(stderr, "client: USAGE: client <server_IP> <server_Port> file1 file2 ...\n");
        exit(EXIT_FAILURE);
    }

    /* Parse and validate port number */
    port = atoi(argv[2]);
    if(port < MIN_PORT || port > MAX_PORT) {
        fprintf(stderr, "client: ERROR: Port number is privileged.\n");
        exit(EXIT_FAILURE);
    }

    /* Install signal handler */
    signal(SIGINT, SIGINT_handler);

    /* Allocate buffer */
    buffer = (char *)malloc(BUFFER_SIZE);
    if(buffer == NULL) {
        fprintf(stderr, "client: ERROR: Failed to allocate memory.\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    /* Process each file from command line */
    for(i = 3; i < argc; i++) {
        /* Create socket for each new connection */
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd < 0) {
            fprintf(stderr, "client: ERROR: Failed to create socket.\n");
            cleanup();
            exit(EXIT_FAILURE);
        }

        /* Setup server address structure */
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        
        if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "client: ERROR: Invalid IP address.\n");
            cleanup();
            exit(EXIT_FAILURE);
        }

        /* Connect to server */
        printf("client: Connecting to %s:%d...\n", argv[1], port);
        if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            fprintf(stderr, "client: ERROR: connecting to %s:%d\n", argv[1], port);
            cleanup();
            exit(EXIT_FAILURE);
        }
        
        printf("client: Success!\n");

        /* Open the file */
        printf("client: Sending: \"%s\"...\n", argv[i]);
        fd_in = open(argv[i], O_RDONLY);
        if(fd_in < 0) {
            fprintf(stderr, "client: ERROR: Failed to open: %s\n", argv[i]);
            close(sockfd);
            sockfd = -1;
            continue;  /* Try next file */
        }

        /* Read file */
        bytes_read = read(fd_in, buffer, BUFFER_SIZE);
        if(bytes_read < 0) {
            fprintf(stderr, "client: ERROR: Unable to read: %s\n", argv[i]);
            close(fd_in);
            fd_in = -1;
            close(sockfd);
            sockfd = -1;
            continue;  /* Try next file */
        }

        /* Send file data */
        if(send(sockfd, buffer, bytes_read, 0) != bytes_read) {
            fprintf(stderr, "client: ERROR: While sending data.\n");
        } else {
            printf("client: Done.\n\n");
        }

        /* Cleanup for next iteration */
        close(fd_in);
        fd_in = -1;
        close(sockfd);
        sockfd = -1;
    }

    printf("client: File transfer(s) complete.\n");
    printf("client: Goodbye!\n");

    cleanup();
    return EXIT_SUCCESS;
}