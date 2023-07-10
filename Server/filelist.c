#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
	DIR *dp;
	struct dirent *dir;
	struct stat sb;

	if((dp = opendir(".")) == NULL)
	{
		perror("directory open error\n");
		exit(1);
	}

	while( (dir = readdir(dp)) != NULL)
	{
		if((dir->d_ino == 0) || !(strcmp(dir->d_name,".")) || !(strcmp(dir->d_name,".."))) 
			continue;
		if(stat(dir->d_name, &sb) == -1)
		{
			printf("%s stat error\n", dir->d_name);
			exit(-1);
		}
		
		printf("%-20s%lld\n",dir->d_name, sb.st_size);  
	}
	
	return 0;
}
