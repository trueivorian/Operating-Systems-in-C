#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define BUFFERLENGTH 256
/* displays error messages from system calls */
void error(char *msg)
{
    perror(msg);
    exit(1);
}
int main(int argc, char *argv[])
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sockfd, s;
    char buffer[BUFFERLENGTH];
    char *b = buffer;
    size_t buffer_len = BUFFERLENGTH;
    int n;

    if (argc != 3)
    {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(1);
    }
    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */
    s = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(1);
    }
    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sockfd = socket(rp->ai_family, rp->ai_socktype,
                        rp->ai_protocol);
        if (sockfd == -1)
            continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; /* Success */
        close(sockfd);
    }
    if (rp == NULL)
    { /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        close(sockfd);
        exit(1);
    }
    freeaddrinfo(result); /* No longer needed */
    bzero(b, BUFFERLENGTH);

    //int loopCount = 0;
    while(getline(&b, &buffer_len, stdin)>0)
    {
        /* send message */
        n = write(sockfd, b, strlen(b));

        if (n < 0){
            close(sockfd);
            error("ERROR writing to socket");
        }
        bzero(b, buffer_len);
    }

    close(sockfd);

    return 0;
}
