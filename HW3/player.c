#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>

//Const variables
#define MUTEX 0
#define INIT 1
#define TURNS 2
#define POTATO 3
#define TURN_NUMBER 4
#define FIFO_TAKE 5
#define READY_PROS 6
#define SENDER 7

//Variables
char* my_fifo;
char* recv;
int fd;
int end_process;
int stringlen;
char readbuf[80];
char writebuf[80];
char end_str[10];
int to_end;
const char* shared_mem;
int* mapped;
char** target_fifos;
char* my_fifo;
int i;
sem_t* sem;
int potato;
int i_sem;
int i_fifos;
int i_mem;
int my_ff_index;

//Functions
void* shared_memory();
void print_data(char*);
void read_pipe();
void write_pipe(char*, char*);
void lock();
void unlock();
void finish(int);
int get_rand(int);
void get_sem(char*);
void ready();
void set_sender();
char* get_sender();

//For reading file line by line
int read_line(int fd, char* buf, int size)
{
	char ch = '\0';
	int len;
	int i = 0;
	for(; i < size; )
	{
		read(fd, &ch, 1);
		if(ch == '\n' || ch == '\0')
		{
			if(i ==0) len = 0;
			break;
		}
		else
		{
			buf[i] = ch;
    	    ch = '\0';
			len = i+1;
		}
		i++;
	}
	buf[i] = '\0';
	return len;
}

int main(int argc, char** argv)
{	
	//Handle terminate command
	signal(SIGINT, finish);
	
	//If not enough arguments provided
	if(argc < 9)
	{
		printf("Wrong args! \n Example: ");
		printf("./player -b haspotatoornot -s nameofsharedmemory -f filewithfifonames -m namedsemaphore\n");
		return -1;
	} 
	
	//Assign arguments
	
	int _b = 1, _s = 1, _f = 1, _m = 1;
	
	for(int _i = 1; _i < 8; _i += 2)
	{
		if(argv[_i][0] != '-')
		{
			printf("Wrong args! \n Example: ");
			printf("./player -b haspotatoornot -s nameofsharedmemory -f filewithfifonames -m namedsemaphore\n");
			return -2;
		}
		
		switch(argv[_i][1])
		{
			case 'b':
				potato = atoi(argv[_i+1]);
				_b = 0;
				break;
			case 's':
				i_sem = _i+1;
				_s = 0;
				break;
			case 'f':
				i_fifos = _i+1;
				_f = 0;
				break;
			case 'm':
				i_mem = _i+1;
				_m = 0;
				break;
			default:
				printf("Wrong args! \n Example: ");
				printf("./player -b haspotatoornot -s nameofsharedmemory -f filewithfifonames -m namedsemaphore\n");
				return -3;
		}
	}
	
	if(_b || _s || _f || _m)
	{
		printf("Wrong args!\n Example: ");
		printf("./player -b haspotatoornot -s nameofsharedmemory -f filewithfifonames -m namedsemaphore\n");
		return -3;
	}

	//Get shared memory
	shared_mem = argv[i_mem];
	if(shared_memory() == MAP_FAILED) return -4;
	
	//get shared semaphore
	get_sem(argv[i_sem]);
	lock();

	//Opening file to get fifo names
	int file = open(argv[i_fifos], O_RDONLY);
	char line[1024];
	
	int targets = -1;
	int fifo_index = 0;
	
	//Count total fifos
	while(read_line(file, line, 1024)) targets++;
	
	target_fifos = (char**) malloc(targets*sizeof(char*));
	
	lseek(file, SEEK_SET, 0);
	
	//Reading fifos and store them to send potato to a random fifo
	while (read_line(file, line, 1024) > 1)
	{
		int length = strlen(line);
		
		if(fifo_index == mapped[FIFO_TAKE])
		{
			my_ff_index = fifo_index;
			my_fifo = malloc(length + 1);
			strncpy(my_fifo, line, length);
			my_fifo[length] = '\0';
		}
		else
		{
			target_fifos[i] = malloc(length + 1);
			strncpy(target_fifos[i], line, length);
			target_fifos[i++][length] = '\0';
		}
		fifo_index++;
	}
	mapped[FIFO_TAKE]++;
	
	close(file);
	
	//for random numbers
	srand(time(NULL));

	//If have potato then send it
	if(potato > 0)
	{
		unlock();
		mapped[POTATO] = getpid();
		mapped[TURNS] = potato;
		mapped[TURN_NUMBER] = 1;
		int rand = get_rand(mapped[FIFO_TAKE]);
		
		printf("pid=%d, sending potato number %d to %s; this is switch number %d\n",
				 getpid(), mapped[POTATO], target_fifos[rand], potato);
		
		sem_wait(sem);
		write_pipe(target_fifos[rand], "potato");
		lock();
	}	
	
	//Ready to get potato
	read_pipe();
	
	finish(0);
}

//If get signal to terminate then terminate all program
void finish(int sig)
{
	if(sig != 0)
	{
		printf("\n");
		for(int j = 0; j < mapped[FIFO_TAKE]-1; j++)
		{
			write_pipe(target_fifos[j], "end");
		}
	}
	shm_unlink(shared_mem);
	remove(my_fifo);
	free(my_fifo);
	
	for(int j = 0; j < i; j++)
	{
		remove(target_fifos[j]);
		free(target_fifos[j]);
	}
	
	sem_close(sem);

	free(target_fifos);
	exit(sig);
}

void* shared_memory()
{
	int size = 10*sizeof(int);
	int shm_fd = shm_open(shared_mem, O_CREAT | O_RDWR, 0666);
	if (shm_fd < 0) return MAP_FAILED;
	
	ftruncate(shm_fd, size);
	
	mapped = (int*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (mapped == MAP_FAILED) return MAP_FAILED;
	if(mapped[INIT] != 1)
	{
		mapped[INIT] = 1;
		mapped[MUTEX] = 0;
	}
	return mapped;
}

void print_data(char* data)
{
	if(!strcmp(data, "potato"))
	{
		printf("pid=%d receiving potato number %d from %s\n"
				,getpid(), mapped[POTATO], get_sender());
		if(mapped[TURN_NUMBER] == mapped[TURNS])
		{
			//Potato cooled down
			printf("pid=%d; potato number %d has cooled down.\n"
					,getpid(), mapped[POTATO]);
			return;
		}
		mapped[TURN_NUMBER]++;
		int rand = get_rand(mapped[FIFO_TAKE]);
		
		sem_wait(sem);
		write_pipe(target_fifos[rand], "potato");
		lock();
	}
}

void read_pipe()
{
	mknod(my_fifo, S_IFIFO|0640, 0);
	strcpy(end_str, "end");
	while(1)
	{
		unlock();
		fd = open(my_fifo, O_RDWR);
		stringlen = read(fd, readbuf, sizeof(readbuf));
		readbuf[stringlen] = '\0';
		
		print_data(readbuf);
		
		to_end = strcmp(readbuf, end_str);
		if (to_end == 0)
		{
			close(fd);
			break;
		}
	}
}

void write_pipe(char* pipe, char* data)
{
	fd = open(pipe, O_CREAT|O_WRONLY);
	strcpy(end_str, "end");
	
	if(!strcmp(end_str, data))
		printf("terminate signal to %s\n", pipe);
	
	strcpy(writebuf, data);
	stringlen = strlen(writebuf);
	
	set_sender();
	
	write(fd, writebuf, strlen(writebuf));
	
	close(fd);
}

void wait()
{
	while(mapped[MUTEX] != 0){}
	mapped[MUTEX] = mapped[FIFO_TAKE]-1;
}

void ready()
{
	mapped[MUTEX] = 0;
}

int get_rand(int upper)
{
	int a = rand();
	return a == 0 ? a : a % (upper > 1 ? upper - 1 : 1);
}

void get_sem(char* name)
{
	char sem_name[strlen(name)+2];
	sprintf(sem_name, "/%s", name);
	sem = sem_open(sem_name, O_CREAT, 0600, 0);
}

void lock()
{
	int val;
	sem_getvalue(sem, &val);
	mapped[READY_PROS]--;
	while(val != 0)
	{
		sem_wait(sem);
		sem_getvalue(sem, &val);
	}
}

void unlock()
{
	int val;
	sem_getvalue(sem, &val);
	mapped[READY_PROS]++;
	sem_post(sem);
}

void set_sender()
{
	mapped[SENDER] = my_ff_index;
}

char* get_sender()
{
	return target_fifos[my_ff_index > mapped[SENDER] ? mapped[SENDER] : mapped[SENDER] - 1];
}
