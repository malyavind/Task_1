#include "mylib.h"

pthread_mutex_t mutex;

int main(int argc, char **argv)
{
	pthread_t tid;
    int sock, slct, received;
    short grnt;
    unsigned tmp = MAX_DELAY + 1;
    char ttime[TIME_SIZE];
    struct sockaddr_in addr;
	time_t ticks;
	msg_type message;
	
	///parameter validation 
	if (argc != 3){
		printf("usage: ./cli <server IP> <username>\n");
		exit(1);
	}
	if (strlen(argv[2]) > MAX_NAME) {
		printf("Имя слишком длинное. Максимальнная длина имени %d\n", MAX_NAME);
		exit(1);
	}
	
	pthread_mutex_init(&mutex, NULL);

	///socket descriptor	
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket");
        exit(2);
    }
	
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT); 
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0) {
		printf("Некорректный IP адрес: %s\n", argv[1]);  /// ip address validation
		exit(3);
	}
    
    ///connection to server    
   if (connect(sock, (SA *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(4);
    }
    received = 0;
	received = recv(sock, &message.todo, sizeof(message.todo), MSG_WAITALL);
	if (received != sizeof(message.todo) && received != 0){
		printf("error getting connection acknowledgment\n");
		exit (1);
	}	
    else 
		printf("connected\n");
	memset(&message, 0, sizeof(message));
    
	///registration or login
	message.todo = LOGIN;
	snprintf(message.from, sizeof(message.from), "%s", argv[2]);
	if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
		perror("send error");
		exit(5);
	}
    received = 0;
	while ( (received = recv(sock, &message, sizeof(message), MSG_WAITALL))) {
		if (received != sizeof(message) && received != 0){
			printf("recv error\n");
			exit (1);
		}
		if(message.todo == TRUE)
			break;
		printf("%s\n", message.msg);
		if(message.todo == FALSE)
			break;
	}
	printf("\n");	
	
	message.todo = sock;
	if(pthread_create(&tid, NULL, ask_for_delivered, &message) != 0) {
		perror("pthread_create");
		syslog(LOG_ERR,"pthread_create");
		exit(4);
	}
	if(pthread_detach(tid) != 0) {
		perror("pthread_detach");
		syslog(LOG_ERR,"pthread_detach");
		exit(5);
	}

    ///interaction with the server
	Print_menu();
	while(1) {
		slct = menu();
		pthread_mutex_lock(&mutex);
		message.garanty = FALSE;
		switch(slct) {
			case 1:///check incoming messages
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				received = 0;
				while ( (received = recv(sock, &message, sizeof(message), MSG_WAITALL))) {
					if (received != sizeof(message) && received != 0){
						printf("recv error\n");
						exit (1);
					}
					if (message.todo == 0) {
						break;
					}	
					else if (message.todo == NO_MESSAGES) {
						printf("Для Вас нет сообщений\n");
						break;
					}	
					else {
						if (message.new_msg == TRUE)
							printf("%s [NEW!] %s: %s\n", message.time, message.from, message.msg);
						else
							printf("%s \t   %s: %s\n", message.time, message.from, message.msg);
							
						if(sendall(sock, (const char*)&message.new_msg, sizeof(message.new_msg)) != sizeof(message.new_msg)) {
							perror("send error");
							exit(5);
						}	
					}	 
				}
				pthread_mutex_unlock(&mutex);
				if (received == 0) {
					printf("server disabled\n");
					exit (1);
				}
				printf("Выберите действие:\n");	
				break;
				
			case 2: ///send message to user
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				
				grnt = 0;
				printf("1 - Отправить с гарантией доставки\n2 - Отправить без гарантии доставки\n");
				while (grnt < 1 || grnt > 2) {
					scanf("%hd", &grnt);
					if (grnt < 1 || grnt > 2) {
						printf("Выберите из предложенных пунктов меню\n");		
					}			
				}	
				
				printf("Введите сообщение:\n");
				clean_stdin();
				fgets(message.msg, MSG_LEN, stdin);
				message.msg[strlen(message.msg) - 1] = '\0';
				printf("Введите имя получателя:\n");
				fgets(message.to, MAX_NAME, stdin);
				tmp = MAX_DELAY + 1;
				printf("Введите задержку отправки в минутах (от 0 до 71582788):\n");

				while (tmp < 0 || tmp > MAX_DELAY) {
					scanf("%u", &tmp);
					if (tmp < 0 || tmp > MAX_DELAY) {
						printf("Некорректная задержка отправки. Повторите ввод\n");		
					}			
				}
				message.delay = tmp * 60;	
				clean_stdin();	
				
				if (grnt == 1) {
					message.garanty = TRUE;
					while (1) {	
						ticks = time(NULL);
						snprintf(ttime, sizeof(ttime), "%s", ctime(&ticks));
						strncpy (message.time, ttime, sizeof(message.time));	
						if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
							perror("send error");
							exit(5);
						}
						strncpy (message.time, "", sizeof(message.time));
						received = 0;
						received = recv(sock, &message.time, sizeof(message.time), MSG_WAITALL);
						if (received != sizeof(ttime)){
							if (received == 0) {
								printf("server disabled\n");
								exit (1);
							}
							printf("recv error\n");
							exit (1);
						}
						if (strncmp(message.time, ttime, sizeof (message.time)) == 0) {
							printf("Сервер подтердил получение сообщения\n");
							message.garanty = FALSE;
							break;
						}	
					}
					received = 0;
					received = recv(sock, &message, sizeof(message), MSG_WAITALL);
					if (received != sizeof(message)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
				}
				else {
					message.garanty = FALSE;
					if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
						perror("send error");
						exit(5);
					}
					received = 0;
					received = recv(sock, &message, sizeof(message), MSG_WAITALL);
					if (received != sizeof(message)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
				}		
				
				printf("%s\n",  message.msg);
				pthread_mutex_unlock(&mutex);
				printf("Выберите действие:\n");	
				break;
			
			case 3: ///send message to group
				
				///sending request for available groups
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				printf("Список доступных групп:\n");
				///getting list of available groups
				received = 0;
				while ( (received = recv(sock, &message, sizeof(message), MSG_WAITALL))) {
					if (received != sizeof(message)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
					if (message.todo == 0)
						break;
					printf("\t%s\n", message.msg);
				}
				message.todo = slct;
								
				grnt = 0;
				printf("1 - Отправить с гарантией доставки\n2 - Отправить без гарантии доставки\n");
				while (grnt < 1 || grnt > 2) {
					scanf("%hd", &grnt);
					if (grnt < 1 || grnt > 2) {
						printf("Выберите из предложенных пунктов меню\n");		
					}			
				}
				
				printf("Введите сообщение:\n");
				clean_stdin();
				fgets(message.msg, MSG_LEN, stdin);
				message.msg[strlen(message.msg) - 1] = '\0';
				
				printf("Введите название группы-получателя:\n");
				fgets(message.to, MAX_NAME, stdin);
				
				tmp = MAX_DELAY + 1;
				printf("Введите задержку отправки в минутах (от 0 до 71582788):\n");
				while (tmp < 0 || tmp > MAX_DELAY) {
					scanf("%u", &tmp);
					if (tmp < 0 || tmp > MAX_DELAY) {
						printf("Некорректная задержка. Повторите ввод\n");		
					}			
				}
				message.delay = tmp * 60;	
				clean_stdin();	
				
				///send with garanty
				if (grnt == 1) {
					message.garanty = TRUE;
					while (1) {	
						ticks = time(NULL);
						snprintf(ttime, sizeof(ttime), "%s", ctime(&ticks));
						strncpy (message.time, ttime, sizeof(message.time));	
						if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
							perror("send error");
							exit(5);
						}
						strncpy (message.time, "", sizeof(message.time));
						received = 0;
						received = recv(sock, &message.time, sizeof(message.time), MSG_WAITALL);
						if (received != sizeof(message.time)){
							if (received == 0) {
								printf("server disabled\n");
								exit (1);
							}
							printf("recv error\n");
							exit (1);
						}
						if (strncmp(message.time, ttime, sizeof message.time) == 0) {
							printf("Сервер подтердил получение сообщения\n");
							message.garanty = FALSE;
							break;
						}	
					}
				}
				else {
					///send without garanty
					message.garanty = FALSE;
					if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
						perror("send error");
						exit(5);
					}
				}	
				///receiving acknowledgment
				received = 0;
				while ( (received = recv(sock, &message, sizeof(message), MSG_WAITALL))) {
					if (received != sizeof(message)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
					if (message.todo == FALSE) {
						printf("%s\n",  message.msg);
						break;
					}	
					else  
						printf("%s\n",  message.msg);
					
					if (message.todo == TRUE)
						break;		
				}	
				pthread_mutex_unlock(&mutex);
				printf("Выберите действие:\n");	
				break;
				
			case 4: ///request for messages status
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				///getting list of delivered messages (delivered = read by addressee)
				received = 0;
				while ( ( received = recv(sock, &message, sizeof(message), MSG_WAITALL))) {
					if (received != sizeof(message)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
					if (message.todo == 0) {
						break;
					}	
					else if (message.todo == NO_MESSAGES) {
						printf("Ни одно из ваших сообщений пока не доставлено\n");
						break;
					}	
					else {
						printf("Ваше сообщение:\n%s\nдля пользователя %s было доставленно в %s\n\n", message.msg, message.to, message.time);					
					}
				}	
				pthread_mutex_unlock(&mutex);
				printf("Выберите действие:\n");			
				break;
			
			case 5: ///join group or create+join
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				printf("Выберите из списка уже существующих групп:\n");
				///getting list of available groups
				received = 0;
				while ( (received = recv(sock, &message, sizeof(message), MSG_WAITALL))) {
					if (received != sizeof(message)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
					if (message.todo == 0)
						break;
					printf("\t%s\n", message.msg);
				}
				///if the group does not exist, it will be created
				printf("Или введите название новой группы:\n");
				message.todo = slct;
				clean_stdin();
				
				fgets (message.msg, MSG_LEN, stdin);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				printf("Отправляем запрос на сервер\n");
				
				received = 0;
				while ( (received = recv(sock, &message.todo, sizeof(message.todo), MSG_WAITALL))) {
					if (received != sizeof(message.todo)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
					if (message.todo == FALSE) {
						printf("Ошибка создания/добавления в группу:\n");
						break;
					}	
					else
						break;
				}
				if (message.todo == FALSE)
					break;
				if (message.todo == TRUE || message.todo == NEW) {
					received = 0;
					received = recv(sock, &message.msg, sizeof(message.msg), MSG_WAITALL);
					if (received != sizeof(message.msg)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
					printf("%s\n", message.msg);
					pthread_mutex_unlock(&mutex);
					printf("Выберите действие:\n");
					break;
				}	
				pthread_mutex_unlock(&mutex);		
				printf("Выберите действие:\n");
				break;
			
			case 6: ///leave group
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				printf("Запрос списка групп, в которых Вы состоите:\n");
				///getting list of your groups
				received = 0;
				while ( (received = recv(sock, &message, sizeof(message), MSG_WAITALL))) {
					if (received != sizeof(message)){
						if (received == 0) {
							printf("server disabled\n");
							exit (1);
						}
						printf("recv error\n");
						exit (1);
					}
					if (message.todo == TRUE) {
						break;
					}
					printf("\t%s\n", message.msg);
					if (message.todo == FALSE)
						break;					
				}
				///if you have not active groups
				if (message.todo == FALSE) {
					pthread_mutex_unlock(&mutex);
					printf("Выберите действие:\n");
					break;
				}
					
				printf("Введите название группы, из которой хотите выйти:\n");	
				message.todo = slct;
				clean_stdin();
				while(1){
					fgets(message.msg, MSG_LEN, stdin);
					if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
					printf("Отправляем запрос на сервер\n");
					///check: are you in this group?
					received = 0;
					while ( (received = recv(sock, &message.todo, sizeof(message.todo), MSG_WAITALL))) {
						if (received != sizeof(message.todo)){
							if (received == 0) {
								printf("server disabled\n");
								exit (1);
							}
							printf("recv error\n");
							exit (1);
						}
						if (message.todo == FALSE) {
							printf("Вы не состоите в выбранной группе, повторите ввод:\n");
							break;
						}	
						else
							break;
					}
					if (message.todo == TRUE) {
						///receiving acknowledgment
						received = 0;
						received = recv(sock, &message.msg, sizeof(message.msg), MSG_WAITALL);
						if (received != sizeof(message.msg)){
							if (received == 0) {
								printf("server disabled\n");
								exit (1);
							}
							printf("recv error\n");
							exit (1);
						}
						printf("%s\n", message.msg);
						break;
					}
							
				}		
				pthread_mutex_unlock(&mutex);
				printf("Выберите действие:\n");
				break;
							
			case 7:
				Print_menu();
			break;
			
			case 8:
				message.todo = EXIT;
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				close(sock);
			return 0;
		}	
	}	
    close(sock);
    return 0;
}
