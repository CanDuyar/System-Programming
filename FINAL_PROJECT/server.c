#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define MAX 2048

//CAN DUYAR - 171044075


typedef char* string;

//For holding temp records
typedef struct linkList {
	string data;
	struct linkList* next;
} LinkList ;

//Single row
typedef struct {
    string* cols;
} Row;

//Table
typedef struct {
    int col;
    int row;
    Row* rows;
} Table;

//Pool thread
typedef struct {
    int index;
    pthread_t id;
    int socktfd;
} PoolThread;

typedef struct {
	pthread_cond_t cond;
	pthread_mutex_t lock;
	int working;
} monitor;

//Args variables
int PORT;
string LOG_FILE;
int POOL;
string DATA_SET;
//Variables
PoolThread* threads;
Table* table;
int sockfd;
//Signal for all thread to terminate
int SIG_TERMINATE = 0;
FILE* log_file;
monitor m;

//Functions
void parse_args(int, char**);
int getColumn(FILE*);
void init_table(string*, Table*, int, int);
void fill_table(string*, Table*);
void free_table(Table*);
void split(string, string*);
void getArray(LinkList*, string*);
void recToStr(LinkList*, string*, int);
void freeString(string*, int);
void recFree(LinkList*);
void write_buff(string, int);
void* client_handler(void*);
void handle(int, int);
int getIndex(char*);
void terminate(int);
static void skeleton_daemon();

void monitor_init(monitor* m) {
	m->working = 0;
	pthread_cond_init(&m->cond, NULL);
	pthread_mutex_init(&m->lock, NULL);
}

void monitor_wait(monitor* m){
	pthread_mutex_lock(&m->lock);
    if (m->working)
    	pthread_cond_wait(&m->cond, &m->lock);
   m->working++;
}

void monitor_signal(monitor* m) {
	pthread_cond_signal(&m->cond);
    m->working--;
    pthread_mutex_unlock(&m->lock);
}


int main(int argc, char** argv)
{
    skeleton_daemon();
    //Parsing arguments
    parse_args(argc, argv);
	//Register signal
	signal(SIGINT, terminate);

    fprintf(log_file, "Loading dataset...\n");
    fflush(log_file);
		clock_t begin = clock();
    float start = time(NULL);
    //Dynamically allocated linked list array
    LinkList* rec = malloc(sizeof(LinkList));
    LinkList* ref = rec;
		ref->data = NULL;
		ref->next = NULL;
	FILE* file = fopen(DATA_SET, "r");
	char line[MAX];
    //Total collumns
	int col = getColumn(file);
	int row = 0;
    //Reading line by line and storing on linked list array
    //Counting rows
	while (fgets(line, sizeof(line), file))
	{
		ref->data = malloc(strlen(line)+1);
		strcpy(ref->data, line);
		row++;
		ref->next = malloc(sizeof(LinkList));
		ref = ref->next;
		ref->data = NULL;
		ref->next = NULL;
	}
	fclose(file);
		clock_t end = clock();
		double time_spent = (double)(end-begin)/CLOCKS_PER_SEC;
    fprintf(log_file, "Dataset loaded in %lf seconds with %d records.\n",time_spent, row-1);
    fflush(log_file);

    string* array = malloc(row*sizeof(string));
		for(int q = 0; q < row;q++){
			array[q] = NULL;
		}
    //Linked list array to char* 1D array convert
    getArray(rec, array);

    //Init Table object and storing all data on that object
    table = malloc(sizeof(Table));
    init_table(array, table, col, row);

    //init monitor
    monitor_init(&m);

    //Socket stuff
    int connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(log_file, "socket creation failed...\n");
        fflush(log_file);
        exit(0);
    }
    bzero(&servaddr, sizeof(servaddr));

    //Assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) {
        fprintf(log_file, "socket bind failed...\n");
        fflush(log_file);
        exit(0);
    }

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        fprintf(log_file, "Listen failed...\n");
        fflush(log_file);
        exit(0);
    }

    //Creating pool threads
    len = sizeof(cli);
    threads = malloc(POOL*sizeof(PoolThread));
    fprintf(log_file, "A pool of %d threads has been created\n", POOL);
    fflush(log_file);
    for (int i = 0; i < POOL; i++) {
        threads[i].index = i;
        threads[i].socktfd = 0;
        pthread_create(&threads[i].id, NULL, client_handler, (void*)&threads[i].index);
    }

    // Accept socket from client and passing to pool thread
    for ( ; ; ) {
        connfd = accept(sockfd, (struct sockaddr*)&cli, (socklen_t*)&len);
        int i = 0;
        int print = 1;
        while (threads[i].socktfd) {
            i++;
            if (i == POOL) {
                i = 0;
                if (print) {
                    print = 0;
                    fprintf(log_file, "No thread is available! Waiting…\n");
                    fflush(log_file);
                }
            }
        }
        threads[i].socktfd = connfd;
    }

    //Cleaning stuff and exit;
    terminate(0);
}

void parse_args(int argc, char** argv)
{
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:o:l:d:")) != -1) {
        switch (opt) {
            case 'p':
            PORT = atoi(optarg);
            break;
            case 'o':
            LOG_FILE = optarg;
            break;
            case 'l':
            POOL = atoi(optarg);
            break;
            case 'd':
            DATA_SET = optarg;
        }
    }
    if (PORT == 0 || LOG_FILE == NULL || POOL < 2 || DATA_SET ==NULL) {
        fprintf(log_file, "USAGE: %s -p PORT -o pathToLogFile –l POOLSize –d datasetPath", argv[0]);
        fflush(log_file);
        exit(1);
    }

    log_file = fopen(LOG_FILE, "w");
    fprintf(log_file, "Executing with parameters:\n-p %d\n-o %s\n-l %d\n-d %s\n", PORT, LOG_FILE, POOL, DATA_SET);
    fflush(log_file);
}

//Get column count
int getColumn(FILE* file)
{
	char ch;
	int col = 0;
	ch = fgetc(file);
    //read a char at a time until '\n'
	while(ch != '\n')
	{
        //If char ',' then a collumn
		if (ch == ',')
			col++;
		ch = fgetc(file);
	}
    //Forward pointer to start
	rewind(file);
	return ++col;
}

//Init table object
void init_table(string* array, Table* table, int col, int row)
{
    table->col = col;
    table->row = row;

    table->rows = malloc(row*sizeof(Row));
    for (int i = 0; i < row; i++) {
        table->rows[i].cols = malloc(col*sizeof(string));
    }

    fill_table(array, table);
}

//Store record to table
void fill_table(string* array, Table* table)
{
    for (int i = 0; i < table->row; i++) {
        split(array[i], table->rows[i].cols);
    }
}

//Free allocated table object
void free_table(Table* table)
{
    for (int i = 0; i < table->row; i++) {
        for (int j = 0; j < table->col; j++)
            if (table->rows[i].cols[j] != NULL)
                free(table->rows[i].cols[j]);

    if (table->rows[i].cols != NULL)
        free(table->rows[i].cols);
    }
    if (table->rows != NULL)
        free(table->rows);
    if (table != NULL)
        free(table);
}

//Split a line text to collumns
void split(string src, string* dest)
{
    int lenth = strlen(src);
    int i = 0, j = 0, l = 0;
    char sub[1024];
		for(int q = 0; q < 1024; q++){
			sub[q] = '\0';
		}
	while (l < lenth) {
		if(src[l] == ',' || src[l] == '\0' || src[l] == '\n') {
			dest[i] = malloc(strlen(sub)+1);
			strcpy(dest[i++], sub);
			bzero(sub, 1024);
			j = 0;
		}
		else {
			sub[j++] = src[l];
		}
		l++;
	}
}

//Link list to 1D array and clean allocated memory
void getArray(LinkList* rec, string* array)
{
    recToStr(rec, array, 0);
    recFree(rec);
}

//Link list to 1D array
void recToStr(LinkList* rec, string* array, int i)
{
    if (rec->data == NULL) return;
    int len = strlen(rec->data);
    rec->data[len-1] = '\0';
    rec->data[len-2] = '\n';
    array[i] = malloc(strlen(rec->data)+1);
    strcpy(array[i], rec->data);
    if (rec->next != NULL) {
        recToStr(rec->next, array, i+1);
    }
}

//Clean allocated linked list
void recFree(LinkList* rec)
{
    if (rec->data != NULL)
        free(rec->data);
    if (rec->next != NULL)
        recFree(rec->next);
    if (rec != NULL)
        free(rec);
}

//Clean allocated 1D array
void freeString(string* array, int size)
{
    for (int i = 0; i < size; i++) {
        free(array[i]);
    }
    free(array);
}

//Write data to client socket and get conformation
void write_buff(string buff, int sockfd)
{
    write(sockfd, buff, strlen(buff)+1);
    bzero(buff, MAX);
    read(sockfd, buff, sizeof(buff));
}

//Pool threads work
void* client_handler(void* data)
{
    int id = *((int*)data);
    monitor_wait(&m);
    fprintf(log_file, "Thread #%d: waiting for connection\n", id);
    fflush(log_file);
    monitor_signal(&m);
    for (;;) {
        if (threads[id].socktfd > 0) {
            monitor_wait(&m);
            fprintf(log_file, "A connection has been delegated to thread id #%d\n", id);
            fflush(log_file);
            monitor_signal(&m);
            handle(id, threads[id].socktfd);
            close(threads[id].socktfd);
            threads[id].socktfd = 0;
        }
        if (SIG_TERMINATE) break;
    }
    return NULL;
}

// Handle a client. Get query and response
void handle(int id, int sockfd)
{
    for (;;) {
        char buff[MAX];
        bzero(buff, MAX);
        //Read the query from client and copy it in buffer
        read(sockfd, buff, sizeof(buff));

        if (!strcmp(buff, "end\n")) break;

        buff[strlen(buff)-1] = '\0';
        monitor_wait(&m);
        fprintf(log_file, "Thread #%d: received query '%s'\n", id, buff);
        fflush(log_file);
        monitor_signal(&m);

        if(!strcmp(buff, "SELECT * FROM TABLE;")) {
            bzero(buff, MAX);
            sprintf(buff, "%d\n", table->col);
            write_buff(buff, sockfd);

            bzero(buff, MAX);
            sprintf(buff, "%d\n", table->row);
            write_buff(buff, sockfd);

            for (int i = 0; i < table->row; i++) {
                for (int j = 0; j < table->col; j++) {
                    bzero(buff, MAX);
                    sprintf(buff, "%s\n", table->rows[i].cols[j]);
                    write_buff(buff, sockfd);
                }
            }
            monitor_wait(&m);
            fprintf(log_file, "Thread #%d: query completed, %d records have been returned\n", id, table->row);
            fflush(log_file);
            monitor_signal(&m);
        }
        else {
            int s = 1;
            for (int i = 0; i < strlen(buff); i++) {
                if (buff[i] == ' ') s++;
            }
            char tokens[s][100];
            char* p = strtok(buff, " ");
            int i = 0;
            while (p != NULL) {
                strcpy(tokens[i++], p);
                p = strtok(NULL, " ");
            }

            if (!strcmp(tokens[0], "SELECT")) {
                if (!strcmp(tokens[1], "DISTINCT")) {
                    bzero(buff, MAX);
                    sprintf(buff, "%d\n", s-4);
                    write_buff(buff, sockfd);

                    bzero(buff, MAX);
                    sprintf(buff, "%d\n", table->row);
                    write_buff(buff, sockfd);
                    for (int i = 0; i < table->row; i++) {
                        for (int j = 2; j < s-2; j++) {
                            int index = getIndex(tokens[j]);
                            if (index != -1) {
                                bzero(buff, MAX);
                                sprintf(buff, "%s\n", table->rows[i].cols[index]);
                                write_buff(buff, sockfd);
                            }
                        }
                    }
                    monitor_wait(&m);
                    fprintf(log_file, "Thread #%d: query completed, %d records have been returned\n", id, table->row);
                    fflush(log_file);
                    monitor_signal(&m);
                }
                else {
                    bzero(buff, MAX);
                    sprintf(buff, "%d\n", s-3);
                    write_buff(buff, sockfd);

                    bzero(buff, MAX);
                    sprintf(buff, "%d\n", table->row);
                    write_buff(buff, sockfd);
                    for (int i = 0; i < table->row; i++) {
                        for (int j = 1; j < s-2; j++) {
                            int index = getIndex(tokens[j]);
                            if (index != -1) {
                                bzero(buff, MAX);
                                sprintf(buff, "%s\n", table->rows[i].cols[index]);
                                write_buff(buff, sockfd);
                            }
                        }
                    }
                    monitor_wait(&m);
                    fprintf(log_file, "Thread #%d: query completed, %d records have been returned\n", id, table->row);
                    fflush(log_file);
                    monitor_signal(&m);
                }
            }
            else {
                //Thinks not implemented. UPDATE
                monitor_wait(&m);

                bzero(buff, MAX);
                sprintf(buff, "%d\n", 0);
                write_buff(buff, sockfd);

                bzero(buff, MAX);
                sprintf(buff, "%d\n", 0);
                write_buff(buff, sockfd);
                fprintf(log_file, "Thread #%d: query not completed, 0 records have been returned\n", id);
                fflush(log_file);
                monitor_signal(&m);
            }
        }
    }
    close(sockfd);
}

int getIndex(char* token)
{
    int len = strlen(token);
    if(token[len-1] == ',') token[len-1] = '\0';
     for (int j = 0; j < table->col; j++) {
        if (!strcmp(table->rows[0].cols[j], token))
            return j;
    }
    return -1;
}

void terminate(int sig)
{
    fprintf(log_file, "\nTermination signal received, waiting for ongoing threads to complete.\nAll threads have terminated, server shutting down.\n");
    fflush(log_file);
    fclose(log_file);
    SIG_TERMINATE = 1;
    for (int i = 0; i < POOL; i++) {
        pthread_join(threads[i].id, NULL);
    }
    close(sockfd);
    free_table(table);
    free(threads);
    exit(0);
}

static void skeleton_daemon()
{
    pid_t pid;

    //Fork off the parent process
    pid = fork();

    // An error occurred
    if (pid < 0)
        exit(EXIT_FAILURE);

     // Success: Let the parent terminate
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // On success: The child process becomes session leader
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    // Catch, ignore and handle signals
    //TODO: Implement a working signal handler
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    // Fork off for the second tim
    pid = fork();

    // An error occurred
    if (pid < 0)
        exit(EXIT_FAILURE);

    // Success: Let the parent terminate
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // Set new file permissions
    umask(0);


    // Close all open file descriptors
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
}
