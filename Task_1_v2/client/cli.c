#include "mylib.h"

int main(int argc, char **argv)
{
    int sock, slct;
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
	if (strlen(argv[2]) > 32) {
		printf("Имя слишком длинное. Максимальнная длина имени 32\n");
		exit(1);
	}
	
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
   /* if (connect(sock, (SA *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(4);
    }*/
    while (1) {
		if (connect(sock, (SA *)&addr, sizeof(addr)) == 0) {
			if (recv(sock, &message.todo, sizeof(message.todo), 0) > 0) {
				if (message.todo == SUCCESS_CONNECT) {
					printf("connected\n");
					break;
				}
			}		
		}
		else
			perror("connect");
		sleep(3);
		printf("retrying to connect\n");
	}
    

	memset(&message, 0, sizeof(message));
    
	///registration or login
	message.todo = LOGIN;
	snprintf(message.from, sizeof(message.from), "%s", argv[2]);
    send(sock, &message, sizeof(message), 0);
	while (recv(sock, &message, sizeof(message), 0)) {
		if(message.todo == TRUE)
			break;
		printf("%s\n", message.msg);
		if(message.todo == FALSE)
			break;
	}

	printf("\n");	
	
    ///interaction with the server
	Print_menu();
	while(1) {
		slct = menu();
		message.garanty = FALSE;
		switch(slct) {
			
			case 1:///check incoming messages
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				//send(sock, &message, sizeof(message), 0);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				while (recv(sock, &message, sizeof(message), 0)) {
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
							
						//send(sock, &message.new_msg, sizeof(message.new_msg), 0);
						if(sendall(sock, (const char*)&message.new_msg, sizeof(message.new_msg)) != sizeof(message.new_msg)) {
						perror("send error");
						exit(5);
					}	
					}	 
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
						//send(sock, &message, sizeof(message), 0);
						if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
							perror("send error");
							exit(5);
						}
						strncpy (message.time, "", sizeof(message.time));
						recv(sock, &message.time, sizeof(message.time), 0);
						if (strncmp(message.time, ttime, sizeof (message.time)) == 0) {
							printf("Сервер подтердил получение сообщения\n");
							message.garanty = FALSE;
							break;
						}	
					}
					recv(sock, &message, sizeof(message), 0);
				}
				else {
					message.garanty = FALSE;
					//send(sock, &message, sizeof(message), 0);
					if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
						perror("send error");
						exit(5);
					}
					recv(sock, &message, sizeof(message), 0);
				}		
				
				printf("%s\n",  message.msg);
				printf("Выберите действие:\n");	
				break;
			
			case 3: ///send message to group
				
				///sending request for available groups
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				//send(sock, &message, sizeof(message), 0);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				printf("Список доступных групп:\n");
				///getting list of available groups
				while (recv(sock, &message, sizeof(message), 0)) {
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
						//send(sock, &message, sizeof(message), 0);
						if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
							perror("send error");
							exit(5);
						}
						strncpy (message.time, "", sizeof(message.time));
						recv(sock, &message.time, sizeof(message.time), 0);
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
					//send(sock, &message, sizeof(message), 0);
					if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				}	
				///receiving acknowledgment
				while (recv(sock, &message, sizeof(message), 0)) {
					if (message.todo == FALSE) {
						printf("%s\n",  message.msg);
						break;
					}	
					else  
						printf("%s\n",  message.msg);
					
					if (message.todo == TRUE)
						break;		
				}	
				
				printf("Выберите действие:\n");	
				break;
				
			case 4: ///request for messages status
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				//send(sock, &message, sizeof(message), 0);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				///getting list of delivered messages (delivered = read by addressee)
				while (recv(sock, &message, sizeof(message), 0)) {
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
				printf("Выберите действие:\n");			
				break;
			
			case 5: ///join group or create+join
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				//send(sock, &message, sizeof(message), 0);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				printf("Выберите из списка уже существующих групп:\n");
				///getting list of available groups
				while (recv(sock, &message, sizeof(message), 0)) {
					if (message.todo == 0)
						break;
					printf("\t%s\n", message.msg);
				}
				///if the group does not exist, it will be created
				printf("Или введите название новой группы:\n");
				message.todo = slct;
				clean_stdin();
				
				fgets (message.msg, MSG_LEN, stdin);
				//send(sock, &message, sizeof(message), 0);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				printf("Отправляем запрос на сервер\n");
				
				while (recv(sock, &message.todo, sizeof(message.todo), 0)) {
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
					recv(sock, &message.msg, sizeof(message.msg), 0);
					printf("%s\n", message.msg);
					break;
				}	
						
				printf("Выберите действие:\n");
				break;
			
			case 6: ///leave group
				message.todo = slct;
				snprintf(message.from, sizeof(message.from), "%s", argv[2]);
				//send(sock, &message, sizeof(message), 0);
				if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
				printf("Запрос списка групп, в которых Вы состоите:\n");
				///getting list of your groups
				while (recv(sock, &message, sizeof(message), 0)) {
					if (message.todo == TRUE) {
						break;
					}
					printf("\t%s\n", message.msg);
					if (message.todo == FALSE)
						break;					
				}
				///if you have not active groups
				if (message.todo == FALSE) {
					printf("Выберите действие:\n");
					break;
				}
					
				printf("Введите название группы, из которой хотите выйти:\n");	
				message.todo = slct;
				clean_stdin();
				while(1){
					fgets(message.msg, MSG_LEN, stdin);
					//send(sock, &message, sizeof(message), 0);
					if(sendall(sock, (const char*)&message, sizeof(message)) != sizeof(message)) {
					perror("send error");
					exit(5);
				}
					printf("Отправляем запрос на сервер\n");
					///check: are you in this group?
					while (recv(sock, &message.todo, sizeof(message.todo), 0)) {
						if (message.todo == FALSE) {
							printf("Вы не состоите в выбранной группе, повторите ввод:\n");
							break;
						}	
						else
							break;
					}
					if (message.todo == TRUE) {
						///receiving acknowledgment
						recv(sock, &message.msg, sizeof(message.msg), 0);
						printf("%s\n", message.msg);
						break;
					}
							
				}		
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
