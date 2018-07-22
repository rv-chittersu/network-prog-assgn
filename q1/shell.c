#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<signal.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>

typedef struct commandInfo{
    char command[99];
    int status;
}info;

typedef struct nodes{
    info *cInfo;
    struct nodes* next;
}node;

node *start;
node *end;
int size = 0;


info *currentCommand;
int status;


void addToQueue(info* i){
    node * newNode = (node *)malloc(sizeof(node));
    newNode->cInfo = i;
    newNode->next = NULL;

    if(size == 0){
        start = newNode;
        end = newNode;
        size++;
    }else if(size < 10){
        end->next = newNode;
        end = newNode;
        size++;
    }else{
        end->next = newNode;
        end = newNode;
        start = start->next;
    }
}

void quitPrompt(){
    printf("\n Do you really want to exit?\n");
    char str[10];
    scanf("%s",str);
    if(strcmp(str,"YES") == 0 || strcmp(str,"yes") == 0){
        printf("closing...\n");
        exit(0);
    }
}

void printHistory(){
    if(size == 0){
        printf("\n");
        printf("No Recent Commands\n");
    }else{
        node *index;
        index = start;
        while(index != NULL){
            printf("\n");
            printf("Command : %s",index->cInfo->command);
            switch(index->cInfo->status){
                case 0:
                    printf("Status : Running\n");
                    break;
                case 1:
                    printf("Status : Success\n");
                    break;
                case -1:
                    printf("Status : Failed\n");
                    break;
            }
            index = index->next;
        }
    }
    printf("\n");
    printf("->");
    fflush(stdout);
}

void sigHandler(int signal){
    if(signal == SIGQUIT){
        quitPrompt();
    }else if(signal == SIGINT){
        printHistory();
    }else if(signal == SIGALRM){
        status = -1;
    }
}

void print(char** argumnets){
	int i = 0;
	while(argumnets[i] != NULL){
		printf("%s\n",argumnets[i]);
		i++;
	}

}

int tokenizer(char **container, char *string, int max_size, int str_size, char *token){
	int index = 0;
	container[max_size - 1] = NULL;
    while(index < max_size - 1){
		char *temp = (char *)malloc(sizeof(char)*str_size);
		char *ptr;

		if(index == 0){
			ptr = strtok(string,token);
		}else{
			ptr = strtok(NULL,token);
		}

		if(ptr == NULL){
			container[index] == NULL;
			break;
		}

		strcpy(temp,ptr);
        container[index] =temp;
        index++;
	}
	return index;
}


int main(){
    char* env_paths = getenv("PATH");

	char path_str[200];
	strcpy(path_str,env_paths);

	char **path = (char **)malloc(sizeof(char *)*20);

	int path_count = tokenizer(path,path_str,20,50,":");

    signal(SIGQUIT,sigHandler);
    signal(SIGINT,sigHandler);
    signal(SIGALRM,sigHandler);
    printf("-------------> STARTING SHELL <-------------\n");
    while(1){
        currentCommand = (info *)malloc(sizeof(info));
        char command[100];
        printf("->");
        fgets(command,100,stdin);
		int i =0;
		
        addToQueue(currentCommand);
        printf("\n");
        strcpy(currentCommand->command,command);
		
        char **arguments = (char **)malloc(sizeof(char *)*20);
		
		tokenizer(arguments,command,20,50," \n");

		int path_index = -1;
		for(; path_index < path_count; path_index++){
			status = 0;
			int pid = fork();
			if(pid == 0){
				if(path_index == -1){
					int res = execv(arguments[0],arguments);
					kill(getppid(),SIGALRM);
					exit(0);
				}
				char filename[100];
				strcpy(filename,path[path_index]);
				strcat(filename,"/");
				strcat(filename,arguments[0]);
				arguments[0] = filename;
				int res = execv(arguments[0],arguments);
				kill(getppid(),SIGALRM);
				exit(0);
			}else{
				wait(NULL);
				if(status == 0){
					printf(" PID %d\n",pid);
					printf(" STATUS : Successs\n");
					currentCommand->status = 1;
					break;
				}
			}
		}

		if(path_index == path_count){
			printf(" PID --\n");
			printf(" STATUS : Failure\n");
			currentCommand->status = -1;
		}
    }
}
