#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/msg.h>




typedef struct stats{
    int *pid;
    int *busy;
    int *count;
}stat;

typedef struct messageBuffer{
	long mtype;
	char mtext[9999];
}msgBuff;

typedef struct logContents{
    char *time_stamp;   
    char *user_ip;      
    char *user_port;    
    char *user_agent;   
    char *request_size;   
    char *query;        
    char *process_time;
    char *HTTP_code;    
}logContent;


void signalHandler(int signal);
void initialise();
void bind_socket();
void startListening(int index);
void generateLog(msgBuff *buffer, logContent *logData);
char *getTime();
void getInfo(logContent *logData, struct sockaddr_in clientaddr, socklen_t addrlen);
void prefork(int fork_count);
void processKiller(int count);
void check();
void check_log();
void respond(int socket_fd, logContent *logData);
int evaluate(logContent* logData, char* message, int message_length);

static stat *stat_var;

static int I = 0;

int max_pool_size = 100;

static int socket_fd;

static int fork_index;

char *ROOT ;

char *port;

int queue_id;

void signalHandler(int signal)
{
	if (signal==14) {
		check();
	}else if(signal == 30){
		int pid = getpid();
		if(stat_var->busy[fork_index] == 0){
			printf("killing - %d\n",stat_var->pid[fork_index]);
			stat_var->pid[fork_index] = -1;
			close(socket_fd);
			exit(0);
		}
	}
}


void initialise(){
    stat_var = mmap(NULL, sizeof *stat_var, PROT_READ | PROT_WRITE,
                                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    stat_var->pid = mmap(NULL, sizeof(int)*max_pool_size, PROT_READ | PROT_WRITE,
                                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    stat_var->busy = mmap(NULL, sizeof(int)*max_pool_size, PROT_READ | PROT_WRITE,
                                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    stat_var->count = mmap(NULL, sizeof(int)*max_pool_size, PROT_READ | PROT_WRITE,
                                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    int i = 0;

    for(i = 0; i < max_pool_size; i++){
        stat_var->pid[i] = -1;
    }

	queue_id = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
	if(queue_id < 0){
		printf("Error creating message queue\n");
		exit(0);
	}
}

//implement binding here
void bind_socket(){

    struct addrinfo *hints,*result,*addr_ptr;
    hints = malloc(sizeof(struct addrinfo));

    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags = AI_PASSIVE;

    if(getaddrinfo( NULL, port, hints, &result) != 0){
        printf("Error getting addrinfo\n");
        exit(1);
    }

    for(addr_ptr = result; addr_ptr != NULL; addr_ptr = addr_ptr->ai_next){
        socket_fd = socket(result->ai_family, result->ai_socktype, 0);
        if(socket_fd == -1){
            continue;
        }
        if (bind(socket_fd, result->ai_addr, result->ai_addrlen) == 0){
            break;
        }else{
            close(socket_fd);
        }
    }

    if(addr_ptr == NULL){
        printf("could not bind\n");
        exit(1);
    }

    freeaddrinfo(result);

    if (listen(socket_fd, 999999) != 0 )
    {
        printf("could not listen");
        exit(1);
    }
}

void startListening(int index){
    while(1){
        if(stat_var->pid[index] != -1){
            break;
        }
    }

	printf("PID - %d | INDEX - %d\n  ",stat_var->pid[index],index);
	fork_index = index;

	msgBuff *buffer = (msgBuff *)malloc(sizeof(msgBuff));
	buffer->mtype = 23;
	//buffer.mtext[0] = (int)'\0';

    while(1){
        // implement request handling

        struct sockaddr_in clientaddr;
        socklen_t addrlen;
        addrlen = sizeof(clientaddr);

        int accepted_socket_fd = accept(socket_fd,(struct sockaddr *)&clientaddr,&addrlen);

        if(accepted_socket_fd >= 0){

            clock_t begin = clock();

            stat_var->busy[index] = 1;

			kill(getppid(),14);

			stat_var->count[index]++;

            logContent *logData = (logContent *)malloc(sizeof(logContent));

            getInfo(logData, clientaddr, addrlen);

            respond(accepted_socket_fd,logData);

            clock_t end = clock();

			char *str = malloc(sizeof(char)*20);
			sprintf(str,"%f",(double)(end - begin) / CLOCKS_PER_SEC);

            logData->process_time = str;

            generateLog(buffer,logData);

			if(msgsnd(queue_id, buffer, sizeof(long) + strlen(buffer->mtext)*(sizeof(char)) + 1, 0) < 0){
				printf("error sending message \n");
			}

			stat_var->busy[index] = 0;

			kill(getppid(),14);
        }

        //killing after receiving signal
        if(stat_var->count[index] == -1){
            //kill self
			printf("killing - %d\n",stat_var->pid[index]);
            stat_var->pid[index] = -1;
            exit(0);
        }
    }
}

void generateLog(msgBuff *buffer, logContent *logData){
   
	char log_string[999];
	log_string[0] = (int)'\0';

	strcat(log_string,logData->time_stamp);
	strcat(log_string,",");


	strcat(log_string,logData->user_ip);
	strcat(log_string,",");


	strcat(log_string,logData->user_port);
	strcat(log_string,",");

	
	strcat(log_string,logData->user_agent);
	strcat(log_string,",");

	strcat(log_string,logData->request_size);
	strcat(log_string,",");


	strcat(log_string,logData->query);
	strcat(log_string,",");



	strcat(log_string,logData->process_time);
	strcat(log_string,",");



	strcat(log_string,logData->HTTP_code);

	char *str = strtok(log_string,"\n");
	strcpy(buffer->mtext,str);
	strcat(buffer->mtext,"\n");
}

char *getTime(){
    time_t rawTime;
    struct tm* timeInfo;

    time ( &rawTime );
    timeInfo = localtime ( &rawTime );

    char *time_string = asctime(timeInfo);
	strtok(time_string,"\n");
	return time_string;
}

void getInfo(logContent *logData, struct sockaddr_in clientaddr, socklen_t addrlen){

    logData->user_ip = inet_ntoa(clientaddr.sin_addr);
	char *buffer = (char *)malloc(sizeof(char)*10);
	sprintf(buffer,"%hu",clientaddr.sin_port);
    logData->user_port = buffer;
    logData->time_stamp = getTime();
}

void prefork(int fork_count){
	printf("forking: %d\n",fork_count);
    int i;
    int index = 0;
    for(i = 0; i < fork_count; i++){
        for(;index < max_pool_size; index++){
            if(stat_var->pid[index] == -1){
                break;
            }
        }
		if(index == max_pool_size){
			return;
		}
        int pid = fork();
        if(pid == 0){
            //child process
            startListening(index);
            break;
        }if(pid == -1){
            // failed forking
            printf("failure to create fork\n");
        }else{
            //parent process
            stat_var->busy[index] = 0;
            stat_var->count[index] = 0;
            stat_var->pid[index] = pid;
        }
        index++;
    }
}

void processKiller(int count){
	printf("Killing: %d\n",count);
    int i,j;
    for(i = 0; i < count; i++){
        int max_index = -1;
        for(j = 0; j < max_pool_size; j++){
            if(stat_var->pid[j] != -1){
                if(max_index == -1 || stat_var->count[j] > stat_var->count[max_index]){
                    max_index = j;
                }
            }
        }
        stat_var->count[max_index] = -1;
		kill(stat_var->pid[max_index],30);
    }
}

void check(){
	
    int free_count = 0;
    int i;
    for(i = 0; i < max_pool_size; i++){
        if(stat_var->pid[i] != -1 && stat_var->busy[i] == 0){
            free_count++;
        }
    }
    if(free_count > I*(1.25)){
        processKiller(free_count - I);
    }else if(free_count < I*(0.75)){
        prefork(I - free_count);
    }
	check_log();
}

void check_log(){
	FILE *log_file = fopen("log.csv","ab+");
	if(log_file == NULL){
		printf("Error creating file\n");
		exit(0);
	}
	while(1){
		msgBuff buffer;
		if(msgrcv(queue_id, &buffer, 9999, 23, IPC_NOWAIT) == -1){
			break;
		}
		fprintf(log_file,"%s",buffer.mtext);
	}
	fclose(log_file);
}

int main(int argc,char** argv){
	signal(14,signalHandler);
	signal(30,signalHandler);
    I = atoi(argv[1]);
	ROOT = argv[2];
	port = argv[3];

    initialise();
    bind_socket();
    prefork(I);
    getchar();
	printf("closing\n");
	return 0;
}

void respond(int socket_fd, logContent *logData){

    char *message = (char*)malloc(sizeof(char)*9999);
    char *data = (char*)malloc(sizeof(char)*1024);
    int i;
    for(i = 0; i < 9999; i++){
        message[i] = (int)'\0';
    }
    int length_of_message = recv(socket_fd, message, 9999, 0);

    int requested_file_fd, len_sent;
	requested_file_fd = evaluate(logData,message,length_of_message);
    if(requested_file_fd < 0){
        write(socket_fd, logData->HTTP_code, strlen(logData->HTTP_code));
    }else{
        send(socket_fd, logData->HTTP_code , strlen(logData->HTTP_code), 0);
        while(len_sent = read(requested_file_fd, data, 1024)){
            write(socket_fd, data, len_sent);
        }
		close(requested_file_fd);
    }

    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);
}

int evaluate(logContent* logData, char* message, int message_length){
    char *file_not_found = "HTTP/1.0 404 Not Found\n";
    char *currupt_request = "HTTP/1.0 400 Bad Request\n";
    char *success = "HTTP/1.0 200 OK\n\n";

    logData->HTTP_code = currupt_request;
    if(message_length <= 0){
        printf("Error receiving\n");
        return -1;
    }else if(message_length == 0){
        printf("Connection closed by client\n");
        return -1;
    }

	char *buffer = (char *)malloc(sizeof(char)*10);
	sprintf(buffer,"%d",message_length);
    logData->request_size = buffer;

	logData->query = strtok(message, "\t\n\r");
	char *ptr;
	while(1){
		ptr = strtok(NULL," \t\r\n");
		if(ptr == NULL){
			return -1;
		}else if(strncmp(ptr,"User-Agent:", 11) == 0){
			logData->user_agent = strtok(NULL," \t\n\r");
			break;
		}
	}

    char **tokens = malloc(sizeof(char*)*3);
	char *first_line = (char *)malloc(sizeof(char)*99);

	strcpy(first_line,logData->query);

    tokens[0] = strtok(first_line," \t\n");
    int i;
    for(i = 1; i < 3; i++){
        tokens[i] = strtok(NULL," \t\n");
        if(tokens[i] == NULL){
            return -1;
        }
    }

	
    if(strncmp(tokens[0], "GET\0", 4 ) != 0 || ( strncmp( tokens[2], "HTTP/1.0", 8) != 0 &&  strncmp( tokens[2], "HTTP/1.1", 8) != 0)){
        return -1;
    }	

    if(strncmp(tokens[1], "/\0", 2) == 0){
        tokens[1] == "/index.html";
    }

    char *path = (char*)malloc(sizeof(char)*999);
	path[0] = (int)'\0';
    strcat(path,ROOT);
    strcat(path,tokens[1]);


    int fd = open(path, O_RDONLY);

    if(fd < 0){
        logData->HTTP_code = file_not_found;
        return -1;
    }
	
    logData->HTTP_code = success;
    return fd;
}
