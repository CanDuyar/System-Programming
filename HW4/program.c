#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <strings.h>

//constants
#define Q 0
#define S 1
#define P 2

//CAN DUYAR - 171044075

//struct for holding students data
typedef struct
{
	pthread_t id;
	char name[256];
	int index, quality, speed, price, 
	earnings, work, worked;
} Student;

//global variables
Student* students;
int* quality_s;
int* speed_s;
int* price_s;
int total_money;
int total_money_const;
int run = 1;
int have_homework_ = 1;
int total_w = 0;
int moneyover = 0;
int prnt = 1;
int total_hw = 0;
int terminate;
int wait_for_init = 1;
char hwbuff;

//functions
void handle_sig(int);
int read_line(int, char *, int);
int split_words(char *, char **);
void sort_students(Student*, int *, int, int);
void* doHomework(void*);
void* h_homework(void*);

int main(int argc, char **argv)
{
	if(argc != 4)
	{
		printf("Usage: %s <homeworkfile> <studentsfile> <money>\n", argv[0]);
		return 1;
	}
	//handle termination signal
	signal(SIGINT, handle_sig);
	//variables
	char *hfile = argv[1];
	char *wfile = argv[2];
	total_money = atoi(argv[3]);
	total_money_const = total_money;

    //opening students file
	int wfd = open(wfile, O_RDONLY);
	if(wfd <= 0)
	{
		printf("Usage: %s <homeworkfile> <studentsfile> <money>\n", argv[0]);
		return 1;
	}

	char buff[1024];
	while (read_line(wfd, buff, 1024))
		total_w++;
	lseek(wfd, SEEK_SET, 0);
	
	//allocation 
	students = malloc(sizeof(Student) * total_w);
	quality_s = malloc(total_w*sizeof(int));
	speed_s = malloc(total_w*sizeof(int));
	price_s = malloc(total_w*sizeof(int));
	
	//spliting words and creating students
	char *words[4] = {0};
	int i = 0;
	while (read_line(wfd, buff, 1024))
	{
		split_words(buff, words);
		if(words[0] == NULL || words[1] == NULL || words[2] == NULL || words[3] == NULL)
		{
			printf("Usage: %s <homeworkfile> <studentsfile> <money>\n", argv[0]);
			return 1;
		}
		strcpy(students[i].name, words[0]);
		students[i].quality = atoi(words[1]);
		students[i].speed = atoi(words[2]);
		students[i].price = atoi(words[3]);
		students[i].index = i;
		students[i].work = '\0';
		students[i].earnings = 0;
		students[i].worked = 0;
		pthread_create(&students[i].id, NULL, doHomework, (void*) &students[i].index);
		
		free(words[0]);
		free(words[1]);
		free(words[2]);
		free(words[3]);
		i++;
	}
	//closing file
	close(wfd);
	
	//sorting students
	for (i = 0; i < total_w; i++)
	{
		quality_s[i] = i;
		speed_s[i] = i;
		price_s[i] = i;
	}
	sort_students(students, quality_s, total_w, Q);
	sort_students(students, speed_s, total_w, S);
	sort_students(students, price_s, total_w, P);
	
	//printing details of students
	printf("%d students-for-hire threads have been created.\n", total_w);
	printf("Name\tQ\tS\tC\n");
	for (i = 0; i < total_w; i++)
	{
		printf("%s %d %d %d\n", students[i].name, students[i].quality, students[i].speed, students[i].price);
	}
	wait_for_init = 0;
	
	//opening homework file and creating w thread to handle
	int hfd = open(hfile, O_RDONLY);
	if(hfd <= 0)
	{
		printf("Usage: %s <homeworkfile> <studentsfile> <money>\n", argv[0]);
		return 1;
	}
	pthread_t gid;
	pthread_create(&gid, NULL, h_homework, (void*) &hfd);

	int cstudent;
	while(have_homework_)
	{
		int job_done = 1;
		int* tmp;
		switch(hwbuff)
		{
			case 'Q':
                tmp = quality_s;
                break;
			case 'S':
                tmp = speed_s;
                break;
			case 'C':
                tmp = price_s;
                break;
            default:
                job_done = 0;
		}
		
        while(job_done)
        {
            if(!run)
            {
                job_done = 0;
                continue;
            }
            
            for(int j = 0; j < total_w; j++)
            {
				cstudent = tmp[j];
                if(!students[cstudent].work)
                {
                    students[cstudent].work = hwbuff;
                    j = total_w;
                    job_done = 0;
                    hwbuff = '\0';
                }
            }
        }
    }
	while(students[cstudent].work != '\0') if(!run) break;
	run = 0;
	
	//wait for all thread to handle
	pthread_join(gid, NULL);
	for(i = 0; i < total_w; i++)
	{
		pthread_join(students[i].id, NULL);
	}
	
	//printing final result and terminate
	printf("Homeworks solved and money made by the students:\n");
	for (i = 0; i < total_w; i++)
	{
		printf("%s %d %d\n", students[i].name, students[i].worked, students[i].earnings);
	}
	printf("Total cost for %d homeworks %dTL\n", total_hw, total_money_const-total_money);
	printf("Money left at H’s account: %dTL\n", total_money);

	handle_sig(0);
}

void handle_sig(int sig)
{
	terminate = sig;

	if(terminate)
	{
		run = 0;
		for(int i = 0; i < total_w; i++)
		{
			pthread_join(students[i].id, NULL);
		}
	}

	free(students);
	free(quality_s);
	free(speed_s);
	free(price_s);
	exit(sig);
}

//Helper functions
int read_line(int fd, char *buf, int size)
{
	char ch = '\0';
	int i = 0;
	for (; i < size;)
	{
		read(fd, &ch, 1);
		if (ch == '\0')
		{
			break;
		}
		else if (ch == '\n')
		{
			buf[i++] = '\0';
			break;
		}
		else
		{
			buf[i++] = ch;
		}
		ch = '\0';
	}
	if (i > 0 && buf[i - 1] != '\0')
		buf[i] = '\0';
	return i;
}

int split_words(char *line, char **words)
{
	char word[1024];
	int n = 0;
	int i = 0;
	for (int j = 0; j < strlen(line); j++)
	{
		if (line[j] == '\0' || line[j] == ' ')
		{
			word[i] = '\0';
			words[n] = malloc(strlen(word) + 1);
			strcpy(words[n], word);
			bzero(word, 1024);
			i = 0;
			n++;
		}
		else
		{
			word[i++] = line[j];
		}
	}
	if (i)
	{
		word[i] = '\0';
		words[n] = malloc(strlen(word) + 1);
		strcpy(words[n], word);
		n++;
	}
	return n;
}

void sort_students(Student* students, int *array, int size, int M)
{
	for (int i = 0; i < size; ++i)
	{
		for (int j = i + 1; j < size; ++j)
		{
			int vi, vj;
			switch(M)
			{
				case Q:
				    vi =  students[array[i]].quality;
				    vj = students[array[j]].quality;
				    break;
				case S:
				    vi =  students[array[i]].speed;
				    vj = students[array[j]].speed;
				    break;
				case P:
				    vi =  students[array[i]].price;
				    vj = students[array[j]].price;
				    break;
				default:
				    printf("Worng mode!\n");
			}
			
			if (M == P && vi > vj)
			{
				int a = array[i];
				array[i] = array[j];
				array[j] = a;
			}
			else if (vi < vj)
			{
				int a = array[i];
				array[i] = array[j];
				array[j] = a;
			}
		}
	}
	return;
}

void* doHomework(void* idv)
{
	while(wait_for_init)
	;
	int id = *(int*)idv;
	int wait = 1;
	
	while(run)
	{
		if(students[id].work)
		{
			if(total_money >= students[id].price)
			{
			    students[id].worked++;
				total_money -= students[id].price;
				students[id].earnings += students[id].price;
                total_hw++;
                int work_time = 6 - students[id].speed;
                work_time = work_time < 0 ? - work_time : work_time;
				printf("%s is solving homework %c for %d, H has %dTL left.\n", students[id].name, students[id].work, students[id].price, total_money);
                sleep(work_time);
			}
			else
			{
				run = 0;
				moneyover = 1;
				break;
			}
			students[id].work = 0;
			wait = 1;
		}
		else if(wait)
		{
			printf("%s is waiting for a homework\n", students[id].name);
			wait = 0;
		}
	}
    if(prnt && !terminate)
    {
        prnt = 0;
        if(moneyover)
        {
            printf("Money is over, closing.\n");
        }
        else
        {
            printf("No more homeworks left or coming in, closing.\n");
        }
    }
    return NULL;
}

void* h_homework(void* fdv)
{
    int fd = *(int*)fdv;
    int havemoney = 1;
    char ch;
	while(read(fd, &ch, 1))
	{
		if(ch == '\n')
		{
			ch = 0;
			continue;
		}
		int job_done = 1;		
		
        while(job_done)
        {
            if(!run)
            {
                job_done = 0;
                continue;
            }
            
			if(hwbuff == '\0')
			{
				printf("H has a new homework %c; remaining money is %dTL\n", ch, total_money);
				hwbuff = ch;
				job_done = 0;
				ch = '\0';
			}
        }
        if(!run)
        {
            printf("H has no more money for homeworks, terminating.\n");
            havemoney = 0;
			have_homework_ = 0;
			run = 0;
            break;
            
        }
    }
	
    if(havemoney && !terminate)
    {
       	printf("H has no other homeworks, terminating.\n");
        have_homework_ = 0;
    }
    return NULL;
}

