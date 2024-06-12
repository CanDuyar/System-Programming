#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>


//CAN DUYAR - 171044075


#define MAX 2048

//Variables
int ID;
char* ADDRESS;
int PORT;
char* QUERY_FILE;

void parse_args(int, char**);
void read_buff(char*, int);
void func(int);

int main(int argc, char** argv)
{
    parse_args(argc, argv);

    //Socket variables
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and varification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }

    bzero(&servaddr, sizeof(servaddr));
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ADDRESS);
    servaddr.sin_port = htons(PORT);

    printf("Client-%d connecting to %s:%d\n", ID, ADDRESS, PORT);
    // connect the client socket to server socket
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    printf("Client-%d connected\n", ID);

    //Function to handle file and SQL commands
    func(sockfd);

    // close the socket
    close(sockfd);
}

//Parsing and verification for arguments
void parse_args(int argc, char** argv)
{
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:a:i:o:")) != -1) {
        switch (opt) {
            case 'p':
            PORT = atoi(optarg);
            break;
            case 'a':
            ADDRESS = optarg;
            break;
            case 'i':
            ID = atoi(optarg);
            break;
            case 'o':
            QUERY_FILE = optarg;
        }
    }
    if (PORT == 0 || ADDRESS == NULL || ID == 0 || QUERY_FILE == NULL) {
        printf("USAGE: %s -i id -a 127.0.0.1 -p PORT -o pathToQueryFile\n", argv[0]);
        exit(1);
    }
}

//Read socket and conform
void read_buff(char* buff, int sockfd)
{
    read(sockfd, buff, MAX);
    write(sockfd, "okay\n", 6);
}

//Open query file and send commands to server
void func(int sockfd)
{
    char buff[MAX];

    FILE* qFile = fopen(QUERY_FILE, "r");
    char line[MAX];

    int query_count = 0;

    //Read file line by line
    while (fgets(line, sizeof(line), qFile)) {
        int id = 0;
        char query[1024];
        //scaning id
        sscanf(line, "%d %[^\n]", &id, query);
        //Comparing id. If not my id then next line
        if (id != ID) continue;
        query_count++;
        printf("sending query '%s'\n", query);
        bzero(buff,MAX);
        strcpy(buff,query);
        buff[strlen(buff)] = '\n';
        clock_t begin = clock();
        float start = time(NULL);
        //Write to server
        write(sockfd, buff, sizeof(buff));
        //Read col
        bzero(buff, sizeof(buff));
        read_buff(buff, sockfd);
        int col = atoi(buff);
        //Read row
        bzero(buff, sizeof(buff));
        read_buff(buff, sockfd);
        int row = atoi(buff);

        //If query not handled then print stats
        if (row == 0 || col == 0) goto label;

        //Dynamic alloc table
        char*** table;
        table = malloc(row*sizeof(char**));
        for (int i = 0; i < row; i++) {
            table[i] = malloc(col*sizeof(char*));
            for (int j = 0; j < col; j++) {
                table[i][j] = malloc(MAX*sizeof(char));
            }
        }

        //Reading table from server
        for (int i = 0; i < row; i++) {
            for (int j = 0; j < col; j++) {
                bzero(buff, sizeof(buff));
                read_buff(buff, sockfd);
                buff[strlen(buff)-1] = '\0';
                strcpy(table[i][j], buff);
            }
        }
        clock_t end = clock();
        double time_spent = (double)(end-begin)/CLOCKS_PER_SEC;

        printf("Server’s response to Client-%d is %d records, and arrived in %lf seconds\n", ID, row-1, time_spent);
        //print results
        for (int i = 0; i < row; i++) {
            printf("%d. ", i);
            for (int j = 0; j < col; j++) {
                printf("%s  ", table[i][j]);
            }
            printf("\n");
        }

        //free table
        for (int i = 0; i < row; i++) {
            for (int j = 0; j < col; j++) {
                free(table[i][j]);
            }
            free(table[i]);
        }
        free(table);
        //skip error message
        continue;

        //error message when query isn't handled
        label:
            printf("Server’s response to Client-%d is 0 records, and arrived in %.2f seconds\n", ID, ((float)start-time(NULL)));
	}

    bzero(buff, MAX);
    strcpy(buff, "end\n");
    write(sockfd, buff, sizeof(buff));
    close(sockfd);
    printf("A total of %d queries were executed, client is terminating\n", query_count);
}
