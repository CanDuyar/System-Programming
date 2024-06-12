#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#define OPT_SIZE 3
#define DIR_NAME_SIZE 2048

#define ANSI_BOLD "\033[4m"
#define ANSI_NO_BOLD "\033[22m"

sig_atomic_t control = 0;
/* struct to store all command line criteria */
typedef struct _criteria {
	int numopt;     /* number of given criteria */
	char *rootDir;   /* directory path to start search from */
	char *filename;  /* filename */
	int bytes;       /* file size in bytes */
	char type;       /* file type (d/s/b/c/f/p/l) */
	char *permission;  /* file permission */
	int numlinks;    /* number of links */
} Criteria;


/* Display usage message to console screen */
void printUsage();

/* Recursively search given directory with given criteria */
int recursiveSearch(char *fromDir, Criteria c, char *lastDir);


/* Match a file entry with given critera. Return 1 for a match, else 0*/
int match_criteria(char *filename, struct stat fileStat, Criteria c);


/* Match given filename with pattern that can optionally have 
 *  + regex symbol. Return 1 if there is a match, else return 0
 */
int match_filename(char *filename, char *pattern);

/* Return the permissions of a file as 9 characters */
void get_permissions(struct stat fileState, char *pm);

void handler(int signal_number)
{
	control=1;
}

int main(int argc, char *argv[])
{
	//	signal handling;
	struct sigaction sig_ac;
	memset(&sig_ac,0,sizeof(sig_ac));
	sig_ac.sa_handler = &handler;
	sigaction(SIGINT,&sig_ac,NULL);

	if((sigemptyset(&sig_ac.sa_mask) == -1) || (sigaction(SIGINT,&sig_ac,NULL) == -1))
		perror("Failed: SIGINT signal handler");

 
	/* command line possible options */
	const char *optstring = "f:b:t:p:l:w:";
	char flag;

	opterr = 0;

	/* create an empty criteria struct */
	Criteria c;
	c.rootDir = NULL;
	c.filename = NULL;
	c.bytes = -1;
	c.type = '.';
	c.permission = NULL;
	c.numlinks = -1;
	c.numopt = 0;

	/* process the command line arguments */
	while(( flag = getopt(argc, argv, optstring)) != -1) {
		if(flag != '?') {
			if(flag == 'w')
				c.rootDir = optarg;
			else if(flag == 'f') {
				c.filename = optarg;
				c.numopt++;
			}
			else if(flag == 'b') {
				c.bytes = atoi(optarg);
				c.numopt++;
			}
			else if(flag == 't') {
				/* check correct file types are given */
				c.type = *optarg;
				if(!(c.type == 'd' || c.type == 's' || c.type == 'b' || c.type == 'c' || c.type == 'f'
					|| c.type == 'p' || c.type == 'l')) {
					fprintf(stderr, "Invalid/Missing options provided!\n");
					printUsage();
					exit(1);
				}
				c.numopt++;
			}
			else if(flag == 'p') {
				c.permission = optarg;
				c.numopt++;
			}
			else if(flag == 'l') {
				c.numlinks = atoi(optarg);
				c.numopt++;
			}
			else {
				fprintf(stderr, "Invalid/Missing options provided!\n");
				printUsage();
				exit(1);
			}
		}
		else {
			fprintf(stderr, "Invalid/Missing options provided!\n");
			printUsage();
			exit(1);
		}

		
	}

	/* Check mandatory -w is provied */
	if(c.rootDir == NULL) {
		fprintf(stderr, "Invalid/Missing options provided!\n");
		printUsage();
		exit(1);
	}

	/* Check at least one criteria is given */
	if(c.numopt == 0) {
		fprintf(stderr, "Invalid/Missing options provided!\n");
		printUsage();
		exit(1);
	}
	
	char lastDir[DIR_NAME_SIZE];
	lastDir[0] = 0;


	/* run search */
	if (!recursiveSearch(c.rootDir, c, lastDir)) {
		printf("No file found\n");
	}


	return 0;
}


void printUsage()
{
	fprintf(stderr,"Usage: ./myFind [OPTION]\n");
	fprintf(stderr,"where OPTION can be any of the following:\n");
	fprintf(stderr,"-f: filename (case insensitive)\n");
	fprintf(stderr,"-b: file size in bytes\n");
	fprintf(stderr,"-t: file type (d: directory, s:socket, b: block, c: character device, f:regular file, p: pipe, l: symbolic link)\n");
	fprintf(stderr,"-p: permissions, as 9 characters (e.g. 'rwxr-xr--')\n");
	fprintf(stderr,"-l: number of links\n");
}

void draw_tree(char *filepath, char *filename, char *lastDir, int lastLevel)
{
	int i, currentLevel = 0, posit = 0, lenFilepath = strlen(filepath);
	if (filepath[0] == '/') currentLevel--; // Ignore first slash

	for (i = 0; i < lenFilepath; i++) {
		if (filepath[i] == '/') {

			posit += 1;
			int lenSub = i - posit;

			if (lenSub > 0) {
				if (currentLevel >= lastLevel) {

					char buff[DIR_NAME_SIZE];
					strncpy(buff, &filepath[posit], lenSub);
					buff[lenSub] = 0;

					/* Skip if this directory is already printed */
					int lastLen = strlen(lastDir);
					if (posit < lastLen && !strncmp(&lastDir[posit], buff, lenSub)) {
						currentLevel++;
						posit = i;
						continue;
					}

					/* display the identation header */
					if (currentLevel) printf("|");
					for(int i = 0; i < currentLevel *2; i++) {
						printf("-");
					}

					if (strstr(filepath, "/")){
						printf("%s\n", buff);
					}
				}
			}

			currentLevel++;
			posit = i;
		}
	}

	printf("|");
	for(int i = 0; i < currentLevel *2; i++) {
		printf("-");
	}

	char ESC = 27;
	printf("%c[1m%s%c[0m\n", ESC, filename, ESC);
}

int recursiveSearch(char *fromDir, Criteria c, char *lastDir)
{
	/* open the start path as directory  */
	DIR *dir = opendir(fromDir);
	struct dirent *dp;
	char filepath[DIR_NAME_SIZE];

	int found = 0;

	/* return if not a directory */
	if(dir == NULL) {
		return found;
	}

	/* process all the file entries of the directory */
	while(( dp = readdir(dir)) != NULL) {
		/* ignore current and parent directory entries */
		if(strcmp(dp->d_name,".") != 0 && strcmp(dp->d_name,"..") != 0) {

			strcpy(filepath,fromDir);
			if (filepath[strlen(filepath) - 1] != '/') strcat(filepath,"/");
			strcat(filepath,dp->d_name);

			struct stat fileStat;
			if(lstat(filepath, &fileStat) < 0) {
				fprintf(stderr, "stat() failed on file %s!\n", filepath);
				exit(1);
			}

			/* match the current file entry with criteria */
			if(match_criteria(dp->d_name, fileStat, c)) {

				int i, lastDirLevel = 0, lenLastDir = strlen(lastDir);

				for (i = 0; i < lenLastDir; i++){
					if (i > strlen(filepath) || lastDir[i] != filepath[i]) break;
					else if (lastDir[i] == '/') lastDirLevel++;
				}

				if (lastDir[0] == '/') lastDirLevel--; // Ignore first slash
				draw_tree(filepath, dp->d_name, lastDir, lastDirLevel);

				strcpy(lastDir, fromDir);
				found = 1;
			}

			/* recursively call with path of current file entry */			
			//sprintf(subdir,"%s/%s",fromDir,dp->d_name);
			if (S_ISDIR(fileStat.st_mode) && recursiveSearch(filepath,c,lastDir)) found = 1;
		}
	}
	closedir(dir);

	if(control == 1){
		fprintf(stderr, "SIGINT received!\n");
		exit(EXIT_SUCCESS);		
	}
	
	return found;
}

int match_filename(char *pattern, char *filename)
{
	int regex = 0, i = 0;
	int lenFilename = strlen(filename);
	int lenPattern = strlen(pattern);
	int lenToken = 0;

    char lowFilename[lenFilename + 1];
    char lowPattern[lenPattern + 1];

    for (i = 0; i < lenFilename; i++)
	{
		if (filename[i] == '+')
		{
			if (!lenToken) lenToken = i;
			regex = 1;
		}

		lowFilename[i] = tolower(filename[i]);
	}

    for (i = 0; i < lenPattern; i++) 
		lowPattern[i] = tolower(pattern[i]);

	lowFilename[lenFilename] = '\0';
	lowPattern[lenPattern] = '\0';

	char *fileptr = lowFilename;

	if (regex)
	{
		char token[lenToken + 1];
		strncpy(token, fileptr, lenToken);
		token[lenToken] = 0;

		char *offset = strstr(lowPattern, token);
		if (offset == NULL) return 0;

		while (lenToken)
		{
			if (strncmp(offset, token, lenToken)) return 0;

			offset += lenToken;
			char skip = *(offset - 1);
			while(*offset == skip) offset++;
			fileptr += lenToken + 1;

			char *search = strstr(fileptr, "+");
			if (search == NULL) return !strncmp(offset, fileptr, strlen(fileptr));
	
			lenToken = search - fileptr;
			strncpy(token, fileptr, lenToken);
			token[lenToken] = 0;
		}
	
		return 1;
	}

	return !strcmp(lowFilename, lowPattern);
}


int match_criteria(char *filename, struct stat fileStat, Criteria c)
{
	/* match filename if option is selected */
	if(c.filename != NULL && !match_filename(filename, c.filename))
		return 0;

	/* match file type if option is selected */
	if(c.type != '.') {
		switch(c.type) {
			case 'f':
			  if(!S_ISREG(fileStat.st_mode))
			  	return 0;
			  break;
			case 'd':
			  if(!S_ISDIR(fileStat.st_mode))
			  		return 0;
			  break;
			case 's':
				if(!S_ISSOCK(fileStat.st_mode))
			  		return 0;
			  break;
			case 'b':
				if(!S_ISBLK(fileStat.st_mode))
			  		return 0;
			  break;
			case 'c':
				if(!S_ISCHR(fileStat.st_mode))
			  	  return 0;
			  break;
			case 'p':
				if(!S_ISFIFO(fileStat.st_mode))
			  	return 0;
			  break;
			case 'l':
				if(!S_ISLNK(fileStat.st_mode))
			  	return 0;
			  break;
		}
	}
	/* match permissions if option is selected */
	if(c.permission != NULL) {
		char filepm[10]; 
		get_permissions(fileStat, filepm);
		if(strcmp(c.permission,filepm) != 0)
			return 0;
	}
	/* match number of links if provided */
	if(c.numlinks > 0 && fileStat.st_nlink != c.numlinks) {
		return 0;
	}
	/* match file size if provided */
	if(c.bytes >= 0 && c.bytes != fileStat.st_size){
		return 0;
	}

	return 1;
}

void get_permissions(struct stat filestat, char *pm)
{
	if(filestat.st_mode & S_IRUSR)
		pm[0] = 'r';
	else
		pm[0] = '-';
	if(filestat.st_mode & S_IWUSR)
		pm[1] = 'w';
	else
		pm[1] = '-';
	if(filestat.st_mode & S_IXUSR)
		pm[2] = 'x';
	else
		pm[2] = '-';


	if(filestat.st_mode & S_IRGRP)
		pm[3] = 'r';
	else
		pm[3] = '-';
	if(filestat.st_mode & S_IWGRP)
		pm[4] = 'w';
	else
		pm[4] = '-';
	if(filestat.st_mode & S_IXGRP)
		pm[5] = 'x';
	else
		pm[5] = '-';

	if(filestat.st_mode & S_IROTH)
		pm[6] = 'r';
	else
		pm[6] = '-';
	if(filestat.st_mode & S_IWOTH)
		pm[7] = 'w';
	else
		pm[7] = '-';
	if(filestat.st_mode & S_IXOTH)
		pm[8] = 'x';
	else
		pm[8] = '-';

	pm[9] = '\0';
}

