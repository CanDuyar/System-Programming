/*CAN DUYAR - 171044075*/

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <signal.h>

#define WORK 0
#define VAC1 1
#define VAC2 2
#define CITIZENS 3

int total_nurses,
    total_vaccinators,
    total_citizens,
    buffer_size,
    dozes;
    
int fd;

int total_vaccines = 0;
//Shared memories
char* vaccines;
char* buffer;
int* citizens;
int* vaccine;
int* data;
int* vacc_data;
int* vacc_data2;

//Semaphores
sem_t* nurses_sem;
sem_t* vaccinators_sem;
sem_t* citizens_sem;
sem_t* nurse_vaccinator_sem;

//Functions
void handle_signal(int);
void printUsage();
void* mem_map(int size);
void nurses_work(int);
char get_vaccine();
int put_vaccine(char);
void vaccinators_work(int);
int* get_citizens();
char get_vaccine1();
char get_vaccine2();
void citizens_work(int);
int register_citizen(int);

int main(int argc, char** argv)
{
	//Handling termination command
    signal(SIGINT, handle_signal);
    char ch;
    char* file;
    //Getting options
    while ((ch = getopt(argc, argv, "n:v:c:b:t:i:")) != -1)
    {
        switch (ch)
        {
            case 'n':
                total_nurses = atoi(optarg);
                break;
            case 'v':
                total_vaccinators = atoi(optarg);
                break;
            case 'c':
                total_citizens = atoi(optarg);
                break;
            case 'b':
                buffer_size = atoi(optarg);
                break;
            case 't':
                dozes = atoi(optarg);
                break;
            case 'i':
                file = optarg;
                break;
        }
    }
    
    //Provided arguments checking
    if(!(total_nurses >= 2 && total_vaccinators >= 2 && total_citizens >= 3 
            && buffer_size >= dozes*total_citizens+1 && dozes >= 1 && file != NULL))
        printUsage();
    
    fd = open(file, O_RDONLY);
    
    
    total_vaccines = 2*total_citizens*dozes;
    
    //mapping memory for shared storage
    //vaccines = mmap(NULL, total_citizens*dozes*2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, fd, 0);
    //vaccines = mem_map(total_vaccines*sizeof(char));
    buffer = mem_map(buffer_size*sizeof(char));
    citizens = mem_map(total_citizens*sizeof(int));
    vaccine = mem_map(total_citizens*sizeof(int));
    data = mem_map(4*sizeof(int));
    data[WORK] = 1;
    data[CITIZENS] = total_citizens;
    vacc_data = mem_map(total_vaccinators*sizeof(int));
    vacc_data2 = mem_map(total_vaccinators*sizeof(int));
    
    
    printf("Welcome to the GTU344 clinic.  Number of citizens to vaccinate c=%d with t=%d dozes.\n",
            total_citizens, dozes);
    
    //Opening named semaphore
    nurses_sem = sem_open("/nurses", O_CREAT, 0644, 1);
    vaccinators_sem = sem_open("/vaccinators", O_CREAT, 0644, 1);
    citizens_sem = sem_open("/citizens", O_CREAT, 0644, 0);
    nurse_vaccinator_sem = sem_open("/nurse_vaccinator", O_CREAT, 0644, 1);
    
    //printf("\n\nadasdcd");
    //Running processes
    citizens_work(total_citizens);
    nurses_work(total_nurses);
    vaccinators_work(total_vaccinators);
    
    //Waiting for child processes to finish
    for(int k = 0; k < total_citizens+total_nurses+total_vaccinators; k++)
    {
        wait(NULL);
    }

    //Remove named semaphore(POSIX Semaphore)
    sem_unlink("/nurses");
    sem_unlink("/vaccinators");
    sem_unlink("/citizens");
    sem_unlink("/nurse_vaccinator");
    
    return 0;
}

void handle_signal(int sig)
{
	//Signal to child processes to terminate
    data[WORK] = 0;
    sem_unlink("/nurses");
    sem_unlink("/vaccinators");
    sem_unlink("/citizens");
    sem_unlink("/nurse_vaccinator");
    exit(0);
}

void printUsage()
{
    printf("n >= 2: the number of nurses (integer)\n");
    printf("v >= 2: the number of vaccinators (integer)\n");
    printf("c >= 3: the number of citizens (integer)\n");
    printf("b >= tc+1: size of the buffer (integer)\n");
    printf("t >= 1: how many times each citizen must receive the 2 shots (integer)\n");
    printf("i: pathname of the input file\n");
    exit(1);
}

void* mem_map(int size)
{
	//Memory map
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

void nurses_work(int n)
{
    for(int i = 0; i < total_nurses; i++)
    {
    	//Creating child process
        int pid = fork();
        if(pid == 0)
        {
        	//Running until signal
            while(data[WORK])
            {//printf("\n\nSA\n\n");
            	//Lock
                sem_wait(nurses_sem);
                
            //printf("\n\nadasdcd");
                char vacc = get_vaccine();
                //When vaccines finished, terminate
                if(!vacc)
                {
                    sem_post(nurses_sem);
                    break;
                }
                int run = 1;
                while(run)
                {
                    sem_wait(nurse_vaccinator_sem);
                    
                    //Put vaccine to buffer
                    if(put_vaccine(vacc))
                    {
                        printf("Nurse %d (pid=%d) has brought vaccine %c: the clinic has %d vaccine1 and %d vaccine2.\n",
                                i, getpid(), vacc, data[VAC1], data[VAC2]);
                        run = 0;
                    }
                    
                    sem_post(nurse_vaccinator_sem);
                }
                
                sem_post(nurses_sem);
            }
            exit(0);
        }
    }
}

//Get vaccine from temp buffer
char get_vaccine()
{
    char vacc = 0;

    while(read(fd, &vacc, sizeof(char)))
    {
        if(vacc == '1' || vacc == '2')
            return vacc;
    }
    return '\0';
}

//Put vaccine to buffer
int put_vaccine(char vacc)
{
    for(int i = 0; i < buffer_size; i++)
    {
    	//If buffer is empty
        if(!buffer[i])
        {
            buffer[i] = vacc;
            if(vacc == '1') data[VAC1]++;
            else if(vacc == '2') data[VAC2]++;
            return 1;
        }
    }
    
    return 0;
}

void vaccinators_work(int n)
{
    for(int v = 0; v < total_vaccinators; v++)
    {	//Creating child processes for vaccinators
        int pid = fork();
        if(pid == 0)
        {
            vacc_data2[v] = getpid();
        	//Get citizens by sorted by their age. small pid is older(For extra)
            int* citizens_line = get_citizens();
            while(data[WORK])
            {
                sem_wait(vaccinators_sem);
                sem_wait(nurse_vaccinator_sem);
                if(data[VAC1] > 0 && data[VAC2] > 0)
                {
                    int finish_v = 1;
                    //Getting two vaccines
                    char vacc1 = get_vaccine1();
                    char vacc2 = get_vaccine2();
                    if(!vacc1 || !vacc2)
                    {
                        //printf("\n\nVac NULL\n\n");
                        
                        sem_post(nurse_vaccinator_sem);
                        sem_post(vaccinators_sem);
                        break;
                    }
                    //Then finding a citizen
                    for(int i = 0; i < total_citizens; i++)
                    {
                        if(vaccine[citizens_line[i]] < dozes)
                        {
                            printf("Vaccinator %d (pid=%d) is inviting citizen pid=%d to the clinic.\n",
                                    v, getpid(), citizens[citizens_line[i]]);
                            vaccine[citizens_line[i]]++;
                            vacc_data[v]++;
                            i = total_citizens;
                            finish_v = 0;
                            //synchronozing citizens and vaccinators
                            sem_wait(citizens_sem);
                        }
                    }
                    //If all citizens are finished vaccination
                    if(finish_v)
                    {
                        data[WORK] = 0;
                        printf("All citizens have been vaccinated\n");
                        for(int s = 0; s < total_vaccinators; s++)
                            printf(" Vaccinator %d (pid=%d) vaccinated %d doses.", s, vacc_data2[s], vacc_data[s]);
                        printf(" The clinic is now closed. Stay healthy.");
                    }
                }
                
                sem_post(nurse_vaccinator_sem);
                sem_post(vaccinators_sem);
            }
            free(citizens_line);
            exit(0);
        }
    }
}

//Sorted citizens
int* get_citizens()
{
    int* citis = malloc(total_citizens*sizeof(int));
    int* citii = malloc(total_citizens*sizeof(int));
    for(int i = 0; i < total_citizens; i++)
    {
        citii[i] = i;
        citis[i] = citizens[i];
    }
    
    //Sorting by their pid 
    for (int i = 0; i < total_citizens; ++i) 
    {
        for (int j = i + 1; j < total_citizens; ++j)
        {
            if (citis[i] > citis[j]) 
            {
                int a =  citis[i];
                citis[i] = citis[j];
                citis[j] = a;
                a = citii[i];
                citii[i] = citii[j];
                citii[j] = a;
            }
        }
    }
    free(citis);
    return citii;
}

//GHetting vaccine 1 from buffer
char get_vaccine1()
{
    for(int i = 0; i < buffer_size; i++)
    {
        if(buffer[i] == '1')
        {
            char vacc = buffer[i];
            buffer[i] = '\0';
            data[VAC1]--;
            return vacc;
        }
    }
    
    return '\0';
}

//Getting vaccine 2 from buffer
char get_vaccine2()
{
    for(int i = 0; i < buffer_size; i++)
    {
        if(buffer[i] == '2')
        {
            char vacc = buffer[i];
            buffer[i] = '\0';
            data[VAC2]--;
            return vacc;
        }
    }
     
    return '\0';
}

//Citizens work
void citizens_work(int n)
{
    for(int i = 0; i < total_citizens; i++)
    {
        int pid = fork();
        if(pid == 0)
        {
            int my_dozes = 0;
            //Storing their pid to to share with vaccinators
            int id = register_citizen(getpid());
            
            while(data[WORK])
            {
            	//When all dozes completed
                if(vaccine[id] == dozes)
                {
                    printf("Citizen %d (pid=%d) is vaccinated for the %d time: the clinic has %d vaccine1 and %d vaccine2. The citizen is leaving. Remaining citizens to vaccinate: %d\n",
                            i, getpid(), vaccine[id], data[VAC1], data[VAC2], --data[CITIZENS]);
                    
		    		if(data[CITIZENS] == 0)
						data[WORK] = 0;

                    sem_post(citizens_sem);
                    
                    break;
                }
                //if vaccinated then print 
                if(vaccine[id] != my_dozes)
                {
                    my_dozes = vaccine[id];
                    printf("Citizen %d (pid=%d) is vaccinated for the %d time: the clinic has %d vaccine1 and %d vaccine2\n",
                            i, getpid(), vaccine[id], data[VAC1], data[VAC2]);
                    
                    sem_post(citizens_sem);
                }
            }
            exit(0);
        }
    }
}

int register_citizen(int pid)
{
    for(int i = 0; i < total_citizens; i++)
    {
        if(!citizens[i])
        {
        	//storing pid to shared memory
            citizens[i] = pid;
            return i;
        }
    }
    
    return 0;
}
