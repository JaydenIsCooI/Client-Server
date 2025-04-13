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
int server_fd = -1;   /* Server socket descriptor */
int client_fd = -1;   /* Client socket descriptor */
int fd_out = -1;      /* Output file descriptor */
char *buffer = NULL;  /* Buffer for file data */

/* Function prototypes */
void cleanup(void);
void SIGINT_handler(int sig);

/* Cleanup function */
void cleanup(void) {
    /* Free ONLY if it's safe to do so... */
    if(buffer != NULL) {
        free(buffer);         /* Release the memory */
        buffer = NULL;        /* Mark the pointer so that we know we have
                                already released it and avoid a 'dangling
                                pointer. */
    }

    /* Free file descriptor */
    if(fd_out > -1) {
        close(fd_out);        /* Close the file */
        fd_out = -1;          /* Mark it as such */
    }

    /* Free client socket descriptor */
    if(client_fd > -1) {
        close(client_fd);     /* Close the socket */
        client_fd = -1;       /* Mark it as such */
    }

    /* Free server socket descriptor */
    if(server_fd > -1) {
        close(server_fd);     /* Close the socket */
        server_fd = -1;       /* Mark it as such */
    }
}

/* SIGINT handler for the server */
void SIGINT_handler(int sig) {

   /* Issue an error */
   fprintf(stderr, "server: Server interrupted.  Shutting down.\n");

   /* Cleanup after yourself */
   cleanup();

   /* Exit for 'reals' */
   exit(EXIT_FAILURE);

} /* end SIGINT_handler() */

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int port;
    char filename[32];
    int file_counter;
    ssize_t bytes_read;
    int opt;

    /* Initialize variables */
    client_len = sizeof(client_addr);
    file_counter = 1;
    opt = 1;

    /* Command line validation */
    if(argc != 2) {
        fprintf(stderr, "server: USAGE: server <listen_Port>\n");
        exit(EXIT_FAILURE);
    }

    /* Parse and validate port number */
    port = atoi(argv[1]);
    if(port < MIN_PORT || port > MAX_PORT) {
        fprintf(stderr, "server: ERROR: Port number is privileged.\n");
        exit(EXIT_FAILURE);
    }

    /* Install signal handler */
    signal(SIGINT, SIGINT_handler);

    /* Allocate buffer */
    buffer = (char *)malloc(BUFFER_SIZE);
    if(buffer == NULL) {
        fprintf(stderr, "server: ERROR: Failed to allocate memory.\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    /* Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        fprintf(stderr, "server: ERROR: Failed to create socket.\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    /* Set socket options for reuse */
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt))) {
        fprintf(stderr, "server: ERROR: setsockopt failed.\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  /* Hardcoded to localhost */
    server_addr.sin_port = htons(port);

    /* Bind socket */
    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "server: ERROR: Failed to bind socket.\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    /* Listen for connections */
    if(listen(server_fd, 32) < 0) {
        fprintf(stderr, "server: ERROR: listen(): Failed.\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    printf("server: Awaiting TCP connections over port %d...\n", port);

    /* Main server loop */
    while(1) {
        /* Accept connection */
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if(client_fd < 0) {
            fprintf(stderr, "server: ERROR: While attempting to accept a connection.\n");
            continue;
        }

        printf("server: Connection accepted!\n");
        printf("server: Receiving file...\n");

        /* Receive data with MSG_WAITALL flag */
        bytes_read = recv(client_fd, buffer, BUFFER_SIZE, MSG_WAITALL);
        
        if(bytes_read < 0) {
            fprintf(stderr, "server: ERROR: Reading from socket.\n");
            close(client_fd);
            client_fd = -1;
            continue;
        }

        printf("server: Connection closed.\n");

        /* Create output file */
        sprintf(filename, "file-%02d.dat", file_counter);
        printf("server: Saving file: \"%s\"...\n", filename);

        fd_out = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
        if(fd_out < 0) {
            fprintf(stderr, "server: ERROR: Unable to create: %s\n", filename);
            close(client_fd);
            client_fd = -1;
            continue;
        }

        /* Write received data to file */
        if(write(fd_out, buffer, bytes_read) != bytes_read) {
            fprintf(stderr, "server: ERROR: Unable to write: %s\n", filename);
        }

        printf("server: Done.\n\n");

        /* Cleanup for next iteration */
        close(fd_out);
        fd_out = -1;
        close(client_fd);
        client_fd = -1;
        file_counter++;

        printf("server: Awaiting TCP connections over port %d...\n", port);
    }

    /* Cleanup just in case */
    cleanup();
    return EXIT_SUCCESS;
}