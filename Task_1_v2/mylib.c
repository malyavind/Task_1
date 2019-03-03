#include "mylib.h"

pthread_mutex_t mutex;

void *poll_connection (void *args) {
	data_type *data = (data_type *)args;
	msg_type message;
	int ret, i, received, connfd;
	int first_fd, last_fd;
	unsigned short need_new_thread = FALSE;
	unsigned short empty_thread;
	unsigned short main_thread_flag = data->main_thread_flag;
	char id[sizeof(message.id)];
	MYSQL_ROW row;
	MYSQL_RES *res;
	
	data->main_thread_flag = FALSE;						///next threads can be disabled
	
	first_fd = data->firstfd;							///borders of useable
	last_fd = first_fd + data->users_per_thread;		///sockets for next thread
	
	while(1) {
		ret = poll(data->fds, MAX_CL + 1 , -1);
		if(ret < 0) {
			perror("polling");
		}
		if(ret == 0) {
			printf("timeout\n");
			syslog(LOG_INFO,"timeout\n");
		}	
		else {
			if (data->fds[0].revents & POLLIN) {  ///new client connection
				if (Is_this_thr_full(first_fd, data->users_per_thread, data) == FALSE || Is_next_thr_free(last_fd, data->users_per_thread, data) == TRUE) {
					data->fds[0].revents = 0;
					connfd = accept(data->listener, NULL, NULL);
					if(connfd < 0) {
						perror("accept");
						syslog(LOG_ERR,"accept error");
						exit(6);
					}
					need_new_thread = TRUE;
					for (i = first_fd; i < last_fd; i++) {
						if (data->fds[i].fd < 0) {
							data->fds[i].fd = connfd;
							need_new_thread = FALSE;
							break;
						} 
					}
					if (i == last_fd) {
						for (i = 1; i < MAX_CL; i++) {
							if (data->fds[i].fd < 0) {
								data->fds[i].fd = connfd;
								data->firstfd = i;
								break;
							}
						}
						data->fds[i].fd = connfd;
					}
					data->clients++;
					if (data->clients > MAX_CL) {
						perror("too many clients");
						syslog(LOG_ERR,"too many clients");
						exit (15);
					}
					data->fds[i].events = POLLIN;
					message.todo = SUCCESS_CONNECT;
					//send(data->fds[i].fd, &message.todo, sizeof(message.todo), 0);
					sendall(data->fds[i].fd, (const char*)&message.todo, sizeof(message.todo));

					syslog(LOG_INFO,"Клиент присоеденился к сокету %d\n", data->fds[i].fd);
					
					///registration or login
					received = 0;
					received = recv(data->fds[i].fd, &message, sizeof(message), MSG_WAITALL);
					if (received != sizeof(message) && received != 0)
						syslog(LOG_ERR,"recv error on sock %d\n", data->fds[i].fd);
					if (Mysql_check_user(mysql, message.from)) {
						snprintf(message.msg, (sizeof(message.msg)), "Вы зарегестрированы как %s", message.from);
					}
					else {
						snprintf(message.msg, (sizeof(message.msg)), "Добро пожаловать, %s\n", message.from);	
					}
					//send(data->fds[i].fd, &message, sizeof(message), 0);
					sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
						
					///sending info about groups
					res = Find_users_groups(mysql, message.from);
					if (mysql_num_rows(res) == 0) {
						strncpy (message.msg, "Вы не состоите ни в одной группе", sizeof(message.msg));
						message.todo = FALSE;
						//send(data->fds[i].fd, &message, sizeof(message), 0);
						sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
					}
					else {
						strncpy (message.msg, "Вы состоите в следующих группах", sizeof(message.msg));
						//send(data->fds[i].fd, &message, sizeof(message), 0);
						sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
						while( (row = mysql_fetch_row(res))) {
							snprintf(message.msg, sizeof(message.msg), "%s", row[0]);
							//send(data->fds[i].fd, &message, sizeof(message), 0);
							sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
						}
						message.todo = TRUE;
						//send(data->fds[i].fd, &message, sizeof(message), 0);
						sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
					}
					mysql_free_result(res);
					
					if (need_new_thread == TRUE){
						//close(connfd);
						if(pthread_create(&data->tid[i - data->users_per_thread], NULL, poll_connection, data) != 0) {
							perror("pthread_create");
							syslog(LOG_ERR,"prthread_create error");
							exit(4);
						}		
						if(pthread_detach(data->tid[i - data->users_per_thread]) != 0) {
							perror("pthread_detach");
							syslog(LOG_ERR,"pthread_detach error");
							exit(5);
						}
						syslog(LOG_INFO,"Сейчас на сервере %d клиентов.\n\t\tДля нового клиента cоздан новый поток\n", data->clients);
					}				
				}
			}
			else {
				for (i = first_fd; i < last_fd; i++) {
					if (data->fds[i].revents & POLLIN) { ///message from client
						data->fds[i].revents = 0;
						received = 0;
						received = recv(data->fds[i].fd, &message, sizeof(message), MSG_WAITALL);
						if (received != sizeof(message) && received != 0)
							syslog(LOG_ERR,"recv error on sock %d\n", data->fds[i].fd);

						if (message.garanty == TRUE)
							//send(data->fds[i].fd, &message.time, sizeof(message.time), 0);
							sendall(data->fds[i].fd, (const char*)&message.time, sizeof(message.time));	
						if (received > 0) {
							switch(message.todo) {
								case 1:	///User's request for incoming messages				
									res = Mysql_find_msg(mysql, message.from);
									if (mysql_num_rows(res) == 0) {
										syslog(LOG_INFO,"Нет сообщений для пользователя %s\n", message.from);
										message.todo = NO_MESSAGES;
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									}
									else {
										syslog(LOG_INFO,"Следующие сообщения были отправлены пользователю %s:\n", message.from);
										snprintf(message.to, sizeof(message.to), "%s", message.from);
										while( (row = mysql_fetch_row(res))) {																					
											snprintf(message.time, sizeof(message.time), "%s", row[3]);
											snprintf(message.from, sizeof(message.from), "%s", row[2]);
											snprintf(message.msg, sizeof(message.msg), "%s", row[1]);
											snprintf(message.id, sizeof(message.id), "%s", row[0]);
											
											///Check, is this message already delivered?
											if (Is_msg_delivered(mysql, message.id, message.to) == FALSE)
												message.new_msg = TRUE;
											else
												message.new_msg = FALSE;
											
											syslog(LOG_INFO,"\t%s: %s\t\t%.19s\n", message.from, message.msg, message.time);
											
											//send(data->fds[i].fd, &message, sizeof(message), 0);
											sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
											received = 0;
											received = recv(data->fds[i].fd, &message.new_msg, sizeof(message.new_msg), MSG_WAITALL);
											if (received != sizeof(message.new_msg) && received != 0)
												syslog(LOG_ERR,"recv error on sock %d\n", data->fds[i].fd);
											if (message.new_msg == TRUE) {
												snprintf(id, sizeof(id), "%s", message.id);
												if (Add_to_delivered(mysql, id, message.to) == 0) {
													syslog(LOG_INFO,"Сообщение помечено, как доставленное\n");
												}	
												else {
													syslog(LOG_ERR,"Ошибка подтверждения доставки\n");
												}	
											}
										}							
										message.todo = 0;
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									}
									mysql_free_result(res);								
								break;
									
								case 2:
									if (Is_user_registered(mysql, message.to) == TRUE) {
										///User sent message
										syslog(LOG_INFO,"%s: %s\t to %s\n", message.from, message.msg, message.to);
										
										if (Mysql_insert_msg(mysql, message.from, message.to, message.msg, message.delay, message.garanty) == 0) {
											syslog(LOG_INFO,"Сообщение добавлено в базу\n");
											snprintf(message.msg, sizeof(message.msg), "Сервер отправил сообщение пользователю %s\nС задержкой %u мин.\n", message.to, message.delay / 60);
											//send(data->fds[i].fd, &message, sizeof(message), 0);
											sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
										}	
										else {
											printf("Ошибка добавления в базу\n");
											syslog(LOG_ERR,"Ошибка добавления в базу\n");
											strncpy (message.msg, "Ошибка отправки сообщения", sizeof(message.msg));
											//send(data->fds[i].fd, &message, sizeof(message), 0);
											sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
										}
									}
									else {
										syslog(LOG_INFO,"Получатель %s  не зарегистрирован\n", message.to);
										snprintf(message.msg, sizeof(message.msg), "Получатель %s  не зарегистрирован\n", message.to);
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									}		
									break;
									
								case 3:///User sent message to group
									res = Mysql_find_groups(mysql);
									while( (row = mysql_fetch_row(res))) {
										///Sending list of available groups
										snprintf(message.msg, sizeof(message.msg), "%s", row[0]);
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									}
									mysql_free_result(res);
									message.todo = 0;
									//send(data->fds[i].fd, &message, sizeof(message), 0);
									sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
								
									received = 0;
									received = recv(data->fds[i].fd, &message, sizeof(message), MSG_WAITALL);
									if (received != sizeof(message) && received != 0)
										syslog(LOG_ERR,"recv error on sock %d\n", data->fds[i].fd);
									if (message.garanty == TRUE)
										//send(data->fds[i].fd, &message.time, sizeof(message.time), 0);
										sendall(data->fds[i].fd, (const char*)&message.time, sizeof(message.time));
									
									if(Is_group_exists(mysql, message.to) == TRUE) {
										res = Get_users_fromgroup(mysql, message.to);
										if (mysql_num_rows(res) == 0) {
											syslog(LOG_INFO,"В выбранной группе %s нет пользователей\n", message.to);
											message.todo = FALSE;
											strncpy (message.msg, "В выбранной группе нет пользователей", sizeof(message.msg));
											//send(data->fds[i].fd, &message, sizeof(message), 0);
											sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
											mysql_free_result(res);
											break;
										}		
										strncat(message.msg, "\t[to ", sizeof(message.msg) - strlen(message.msg));
										strncat(message.msg, message.to, sizeof(message.msg) - strlen(message.msg));
										strncat(message.msg, "]", sizeof(message.msg) - strlen(message.msg));
										while( (row = mysql_fetch_row(res))) {	
											snprintf(message.to, sizeof(message.to), "%s", row[0]);
											syslog(LOG_INFO,"%s: %s\t to %s\n", message.from, message.msg, message.to);
												
											if (Mysql_insert_msg(mysql, message.from, message.to, message.msg, message.delay, message.garanty) == 0) {
												syslog(LOG_INFO,"Сообщение добавлено в базу\n");
											}	
											else {
												printf("Ошибка добавления в базу\n");
												syslog(LOG_ERR,"Ошибка добавления в базу\n");
												message.todo = FALSE;
												strncpy (message.msg, "Ошибка отправки сообщения", sizeof(message.msg));
												//send(data->fds[i].fd, &message, sizeof(message), 0);
												sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
											}
										}
										message.todo = TRUE;
										strncpy (message.msg, "Ваше сообщение отправлено", sizeof(message.msg));
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
										mysql_free_result(res);
					
									}		
									else {
										message.todo = FALSE;
										syslog(LOG_INFO,"Группа %s  не создана\n", message.to);
										snprintf(message.msg, sizeof(message.msg), "Группа %s  не создана\n", message.to);
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									}			
								break;
									
								case 4:
									syslog(LOG_INFO,"Запрос статуса доставки сообщений пользователем %s\n", message.from);
									res = Mysql_find_delivered(mysql, message.from);
									if (mysql_num_rows(res) == 0) {
										syslog(LOG_INFO,"Ни одно из сообщений пользователя %s не было доставлено\n", message.from);
										message.todo = NO_MESSAGES;
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									}
									else {
										syslog(LOG_INFO,"Статус доставки сообщений выслан пользователю %s\n", message.from);
										while( (row = mysql_fetch_row(res))) {	
											snprintf(message.time, TIME_SIZE, "%s", row[2]);
											snprintf(message.to, sizeof(message.to), "%s", row[1]);
											snprintf(message.msg, sizeof(message.msg), "%s", row[0]);
											//send(data->fds[i].fd, &message, sizeof(message), 0);
											sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
										}
										message.todo = 0;
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));	
									}	
									mysql_free_result(res);
								break;
								
								case 5:
									syslog(LOG_INFO,"Запрос регистрации в группе пользователем %s\n", message.from);
									res = Mysql_find_groups(mysql);
									while( (row = mysql_fetch_row(res))) {
										///Sending list of available groups
										snprintf(message.msg, sizeof(message.msg), "%s", row[0]);
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									}
									mysql_free_result(res);
									message.todo = 0;
									//send(data->fds[i].fd, &message, sizeof(message), 0);
									sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									received = 0;
									while ((received = recv(data->fds[i].fd, &message, sizeof(message), MSG_WAITALL))) {
										if (received != sizeof(message) && received != 0)
											syslog(LOG_ERR,"recv error on sock %d\n", data->fds[i].fd);
										if (Is_group_exists(mysql, message.msg) == FALSE) {
											if (Create_and_join(mysql, message.msg, message.from)) {
												message.todo = NEW;
												//send(data->fds[i].fd, &message.todo, sizeof(message.todo), 0);
												sendall(data->fds[i].fd, (const char*)&message.todo, sizeof(message.todo));
												break;
											}
											else {
												message.todo = FALSE;
												//send(data->fds[i].fd, &message.todo, sizeof(message.todo), 0);
												sendall(data->fds[i].fd, (const char*)&message.todo, sizeof(message.todo));
												break;
											}		
												
										}
										else {
											message.todo = TRUE;
											//send(data->fds[i].fd, &message.todo, sizeof(message.todo), 0);
											sendall(data->fds[i].fd, (const char*)&message.todo, sizeof(message.todo));
											break;
										}	
									}
									if (message.todo == FALSE)
										break;
									else if (message.todo == NEW) {
										syslog(LOG_INFO,"Пользователь %s вступил в группу %s\n", message.from, message.msg);
										strncpy (message.msg, "Вы присоеденились к группе", sizeof(message.msg));
										//send(data->fds[i].fd, &message.msg, sizeof(message.msg), 0);
										sendall(data->fds[i].fd, (const char*)&message.msg, sizeof(message.msg));
										break;
									}	
									if (Add_to_group(mysql, message.from, message.msg) == TRUE) {
										syslog(LOG_INFO,"Пользователь %s вступил в группу %s\n", message.from, message.msg);
										strncpy (message.msg, "Вы присоеденились к группе", sizeof(message.msg));
										//send(data->fds[i].fd, &message.msg, sizeof(message.msg), 0);
										sendall(data->fds[i].fd, (const char*)&message.msg, sizeof(message.msg));
									}											
									else {
										syslog(LOG_INFO,"Пользователь %s уже состоит в группе %s\n", message.from, message.msg);
										strncpy (message.msg, "Вы уже состоите в данной группе", sizeof(message.msg));
										//send(data->fds[i].fd, &message.msg, sizeof(message.msg), 0);
										sendall(data->fds[i].fd, (const char*)&message.msg, sizeof(message.msg));
									}	
								break;
									
								case 6:
									syslog(LOG_INFO,"Запрос выхода из группы пользователем %s\n", message.from);
									res = Find_users_groups(mysql, message.from);
									if (mysql_num_rows(res) == 0) {
										syslog(LOG_INFO,"Пользователь %s не состоит ни в одной группе\n", message.from);
										strncpy (message.msg, "Вы не состоите ни в одной группе", sizeof(message.msg));
										message.todo = FALSE;
										//send(data->fds[i].fd, &message, sizeof(message), 0);
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
										mysql_free_result(res);
										break;
									}
									else {
										while( (row = mysql_fetch_row(res))) {
											snprintf(message.msg, sizeof(message.msg), "%s", row[0]);
											//send(data->fds[i].fd, &message, sizeof(message), 0);
											sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
										}
										message.todo = TRUE;
										send(data->fds[i].fd, &message, sizeof(message), 0);
									}
									mysql_free_result(res);
									///Geting user's choise
									received = 0;
									while ( (received = recv(data->fds[i].fd, &message, sizeof(message), MSG_WAITALL))) {
										if (received != sizeof(message) || received != 0)
											syslog(LOG_ERR,"recv error on sock %d\n", data->fds[i].fd);
										if (Is_user_ingroup(mysql, message.from, message.msg) == FALSE) {
											message.todo = FALSE;
											//send(data->fds[i].fd, &message.todo, sizeof(message.todo), 0);
											sendall(data->fds[i].fd, (const char*)&message.todo, sizeof(message.todo));
										}
										else {
											message.todo = TRUE;
											//send(data->fds[i].fd, &message.todo, sizeof(message.todo), 0);
											sendall(data->fds[i].fd, (const char*)&message.todo, sizeof(message.todo));
											break;
										}	
									}
									if (Delete_user_fromgroup(mysql, message.from, message.msg) == 0) {
										syslog(LOG_INFO,"Пользоватеель %s вышел из группы %s\n",message.from, message.msg);
										strncpy (message.msg, "Вы вышли из группы", sizeof(message.msg));
										//send(data->fds[i].fd, &message.msg, sizeof(message.msg), 0);
										sendall(data->fds[i].fd, (const char*)&message.msg, sizeof(message.msg));
									}											
									else {
										strncpy (message.msg, "Ошибка выхода из группы", sizeof(message.msg));
										//send(data->fds[i].fd, &message.msg, sizeof(message.msg), 0);
										sendall(data->fds[i].fd, (const char*)&message.msg, sizeof(message.msg));
									}	
								break;
								
								case JUST_ASK:
									res = Mysql_find_delivered(mysql, message.from);
									if (mysql_num_rows(res) == 0) {
										message.todo = FALSE;
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
									}
									else {
										message.todo = TRUE;
										while( (row = mysql_fetch_row(res))) {	
											if (strncmp(row[3], "1", strlen(row[3])) == 0) {
												snprintf(message.time, TIME_SIZE, "%s", row[2]);
												snprintf(message.to, sizeof(message.to), "%s", row[1]);
												snprintf(message.msg, sizeof(message.msg), "%s", row[0]);
												sendall(data->fds[i].fd, (const char*)&message, sizeof(message));
												Mark_as_shown(mysql, row[4]);
												syslog(LOG_INFO, "Статус доставки сообщений выслан пользователю %s\n", message.from);
											}
										}
										message.todo = FALSE;
										sendall(data->fds[i].fd, (const char*)&message, sizeof(message));	
									}	
									mysql_free_result(res);
								break;

								case EXIT:
									///normal disconnect
									data->clients--;
									syslog(LOG_INFO,"Клиент отсоеденился от сокета %d\n\t\tСейчас на сервере %d клиент(а/ов)\n", data->fds[i].fd, data->clients);
									close(data->fds[i].fd);
									memset(&message, 0, sizeof(message));
									data->fds[i].fd = -1; 	///make fd available
									if (main_thread_flag == FALSE) {		///searching for active clients on thread
										empty_thread = TRUE;
										for (i = first_fd; i < last_fd; i++) {
											if (data->fds[i].fd > 0) {
												empty_thread = FALSE;
												break;
											}
										}
										if (empty_thread == TRUE){
											syslog(LOG_INFO,"Все клиенты потока отключились. Поток отключен");	
											return 0;						///shotdown thread if not found
										}	
									}
								break;
									
								
							}		
						}
						else if (received == 0) {
							///ctrl+c disconnect or smthng
							data->clients--;
							syslog(LOG_INFO,"Клиент отсоеденился от сокета %d\n\t\tСейчас на сервере %d клиент(а/ов)\n", data->fds[i].fd, data->clients);
							close(data->fds[i].fd);
							memset(&message, 0, sizeof(message));
							data->fds[i].fd = -1; ///make fd available
							if (main_thread_flag == FALSE) { 			///searching for active clients on thread
								empty_thread = TRUE;					
								for (i = first_fd; i < last_fd; i++) {
									if (data->fds[i].fd > 0) {
										empty_thread = FALSE;
										break;
									}
								}
								if (empty_thread != FALSE) {
									syslog(LOG_INFO,"Все клиенты потока отключились. Поток отключен");	
									return 0;				///shotdown thread if not found
								}
								
							}
							
						}
							
					}
				}
			}	
					
		}					
	}
	return 0;
}

int sendall( int sock, const char * buff, int nBytes) {
	int nLeft = nBytes;
   	int idx = 0;
   	while (nLeft > 0)
   	{
    	int ret = send( sock, &buff[idx], nLeft, 0);
      	if (ret < 0) {
			syslog(LOG_ERR, "send error from sendall\n");
      		return -1;
		} 
	  	nLeft -= ret;
    	idx += ret;
	}
	if (idx != nBytes) {
		syslog(LOG_ERR, "send error from sendall\n");
		return -1;
	}
	return idx;
}

void hdl1(int signum) {
	sigset_t set;
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	MYSQL_RES *res;
	MYSQL_ROW row;
	if (sigemptyset(&set)) {
		syslog(LOG_ERR,"sigemptyset error from SIGUSR2\n");
		exit(1);
	}
	
	if (sigaddset(&set,SIGUSR1)) {
		syslog(LOG_ERR,"sigaddset error from SIGUSR2\n");
		exit(1);
	}
	if (sigprocmask(SIG_BLOCK, &set, NULL)) {
		syslog(LOG_ERR,"sigprocmask error from SIGUSR2\n");
		exit(1);
	} 
	snprintf(sql, sizeof(sql), "SELECT m.message_id, m.message, \
			(SELECT uu.username FROM messages AS mm, users AS uu WHERE \
			mm.message_id = m.message_id AND mm.fromm = uu.user_id) AS fromm, \
			(SELECT uu.username FROM messages AS mm, users AS uu WHERE \
			mm.message_id = m.message_id AND mm.tto = uu.user_id) AS tto, \
			m.time from messages AS m");
			
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);
		fprintf(stderr, "sig: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"sig: %s\n", mysql_error(mysql));
	}
	res = mysql_store_result(mysql);
	while( (row = mysql_fetch_row(res))) {
		syslog(LOG_INFO,"id[%s] %s: %s to %s \tat %s\n", row[0], row[2], row[1], row[3], row[4]);
	}
	printf("\n");
	mysql_free_result(res);
	if (sigprocmask(SIG_UNBLOCK, &set, NULL)) 
       	return;

	return;
}

void hdl2(int signum) {
	sigset_t set;
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	MYSQL_RES *res;
	MYSQL_ROW row;
	if (sigemptyset(&set)) {
		syslog(LOG_ERR,"sigemptyset error from SIGUSR2\n");
		exit(1);
	}	
	if (sigaddset(&set,SIGUSR2)) {
		syslog(LOG_ERR,"sigaddset error from SIGUSR2\n");
		exit(1);
	}	
	if (sigprocmask(SIG_BLOCK, &set, NULL)) {
		syslog(LOG_ERR,"sigprocmask error from SIGUSR2\n");
		exit(1);
	}	
      	
	snprintf(sql, sizeof(sql), "SELECT \
		(SELECT g.groupname FROM groups AS g WHERE g.group_id = ug.group_id) AS groupname, \
		(SELECT u.username FROM users AS u WHERE u.user_id = ug.user_id) AS username \
		FROM mydb.user_group AS ug ");
			
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);
		fprintf(stderr, "SIGUSR2: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"SIGUSR2: %s\n", mysql_error(mysql));
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	
	while( (row = mysql_fetch_row(res))) {
		syslog(LOG_INFO,"user %s is in %s group now\n", row[1], row[0]);
	}
	printf("\n");	
	mysql_free_result(res);
	if (sigprocmask(SIG_UNBLOCK, &set, NULL)) 
		return;

	return;
}



void *ext() {
	char str[MSG_LEN];
	while(1) {
		fgets(str, sizeof(str), stdin);
	
		if (strncmp(str, "q!\n", sizeof (str)) == 0) {
			mysql_close(mysql);
			printf("server disabled\n");
			exit(0);
		}
	}
	
} 

void init(void *args, void *db, char *ip, char *upt) {
	data_type *data = (data_type *)args;
	db_login_type *login = (db_login_type *)db;
	data->protocol = IPPROTO_TCP;
	data->listener = -1;
	data->listener = socket(AF_INET, SOCK_STREAM, data->protocol);
	
	pthread_mutex_init(&mutex, NULL);	
	if (data->listener < 0) {
		perror("socket() failed");
		syslog(LOG_ERR,"socket() failed");
	}
	data->serveraddr.sin_family = AF_INET;
	data->serveraddr.sin_port = htons(PORT);
	if (inet_pton(AF_INET, ip, &data->serveraddr.sin_addr.s_addr) <= 0) {
		printf("Некорректный IP адрес: %s\n", ip);
		syslog(LOG_ERR,"Некорректный IP адрес: %s\n", ip);
		exit(3);
	}
	data->thrs = 1;
	data->clients = 0;
	data->users_per_thread = atoi(upt); 
	if (data->users_per_thread < 1) {
		printf("Некорректный 4 аргумент.\nКол-во пользователей на один поток должно быть больше 0\n");
		exit(1);
	}
	data->main_thread_flag = TRUE;
	data->firstfd = 1;
	data->lastfd = data->firstfd + data->users_per_thread;
	
	//mysql = mysql_init(mysql);
	//mysql_init(&mysql);
	mysql = mysql_init(NULL);
	mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, "srv.c");
	if (!mysql) {
		puts("Init faild, out of memory?");
		syslog(LOG_ERR,"Init faild, out of memory?");
		exit(1);
	}

	if(!mysql_real_connect(mysql, login->ip, login->username, login->password, login->name, login->port, NULL, 0)) {
		fprintf(stderr, "Failed to connect to database: Error: %s\n",mysql_error(mysql));
		syslog(LOG_ERR,"Failed to connect to database: Error: %s\n",mysql_error(mysql));
	}

	else {
		syslog(LOG_INFO,"Data Base connected\n");
	}	
}	

void Logmask(char *logmask) {
	
	
	if (strncmp(logmask, "3", strlen(logmask)) == 0) {
		setlogmask (LOG_UPTO (LOG_INFO));
		
	}
	else if (strncmp(logmask, "2", strlen(logmask)) == 0) {
		setlogmask (LOG_UPTO (LOG_NOTICE));
	}
	else if (strncmp(logmask, "1", strlen(logmask)) == 0) {
		setlogmask (LOG_UPTO (LOG_ERR));
	}
	else {
		printf("Некорректный второй аргумент.\nВыберите уровень логирования 1, 2 или 3\n");
		exit(1);
	}
	openlog("srv",  LOG_PID | LOG_PERROR, LOG_USER);
}

char menu() {
	char slct = '0';
	while (1) {
		scanf("%c", &slct);
		clean_stdin();
		if (slct < '1' || slct > '8') {
			printf("Выберите из предложенных пунктов меню\n");
		}
		else
			break;	
	}
	return slct;
}

void clean_stdin(void)
{
	int c;
	while (c != '\n' && c != EOF) {
		c = getchar();
	} 
}

void Print_menu() {
	printf("Выберите действие:\n\
1 - Показать входящие сообщения\n\
2 - Написать сообщение\n\
3 - Написать сообщение группе\n\
4 - Показать статус доставки сообщений\n\
5 - Присоедениться к группе\n\
6 - Выйти из группы\n\
7 - Показать это меню\n\
8 - Выход\n");
}	

unsigned short Is_next_thr_free(int last, int usr_per_thr, void *args) {
	data_type *data = (data_type *)args;
	int i;
	for (i = last; i < last + usr_per_thr; i++){
		if (data->fds[i].fd > 0)
			return FALSE;
	}
	return TRUE;
}

unsigned short Is_this_thr_full(int first, int usr_per_thr, void *args) {
	data_type *data = (data_type *)args;
	int i;
	for (i = first; i < first + usr_per_thr; i++){
		if (data->fds[i].fd < 0)
			return FALSE;
	}
	return TRUE;
}


unsigned short Mysql_insert_msg(MYSQL *mysql, char *from, char *to, char *message, int delay, unsigned short garanty) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	if (garanty == TRUE) {
		snprintf (sql, sizeof(sql), "INSERT INTO messages(fromm, tto, message, time, delay, garanty) values (\
			(SELECT users.user_id from users WHERE users.username = '%s'), \
			(SELECT users.user_id from users WHERE users.username = '%s'), \
			'%s', NOW(), '%u', '1')", from, to, message, delay);
	}
	else {
		snprintf (sql, sizeof(sql), "INSERT INTO messages(fromm, tto, message, time, delay, garanty) values (\
			(SELECT users.user_id from users WHERE users.username = '%s'), \
			(SELECT users.user_id from users WHERE users.username = '%s'), \
			'%s', NOW(), '%u', '0')", from, to, message, delay);
	}
	pthread_mutex_lock(&mutex);			
	if(mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);	
		fprintf(stderr, "Mysql_insert_msg: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Mysql_insert_msg: %s\n", mysql_error(mysql));
		return -1;
	}
	pthread_mutex_unlock(&mutex);	
	return 0;	
}	

MYSQL_RES *User_by_name(MYSQL *mysql, char *name) {	
	MYSQL_RES *res;
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	
	snprintf (sql, sizeof(sql), "SELECT * from mydb.users WHERE username = '%s'", name);
	
	pthread_mutex_lock(&mutex);
	if(mysql_query(mysql, sql)) {
		fprintf(stderr, "User_by_name: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"User_by_name: %s\n", mysql_error(mysql));
        exit(1);
	} 
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	return res;
}


MYSQL_RES *Mysql_find_msg(MYSQL *mysql, char *to) {
	MYSQL_RES *res;
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];	
	
	snprintf(sql, sizeof(sql), "SELECT m.message_id, m.message, \
		(SELECT uu.username FROM messages AS mm, users AS uu WHERE \
		mm.message_id = m.message_id AND mm.fromm = uu.user_id) AS fromm, \
		m.time from messages AS m, users AS u WHERE u.username = '%s' and \
		m.tto = u.user_id AND NOW() > m.time + m.delay", to);

	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
			fprintf(stderr, "Mysql_find_msg: %s\n", mysql_error(mysql));
			syslog(LOG_ERR,"Mysql_find_msg: %s\n", mysql_error(mysql));
			exit(1);
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	syslog(LOG_INFO,"User %s asking messages\n", to);
	return res;
}

int Mysql_check_user(MYSQL *mysql, char *from) {
	MYSQL_RES *res;
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	res = User_by_name(mysql, from);	
	if (mysql_num_rows(res) == 0) {	
		mysql_free_result(res);
		///new user, need to register
		snprintf(sql, sizeof(sql), "INSERT INTO users(username) values ('%s')", from);
		pthread_mutex_lock(&mutex);
		if (mysql_query(mysql, sql)) {
			pthread_mutex_unlock(&mutex);
			fprintf(stderr, "Mysql_check_user: %s\n", mysql_error(mysql));
			syslog(LOG_ERR,"Mysql_check_user: %s\n", mysql_error(mysql));
			return -1;
		}
		pthread_mutex_unlock(&mutex);
		
		syslog(LOG_INFO,"Новый пользователь %s зарегестрирован\n", from);
		return 1;
			
	}
	else {
		///old user, need to say "hello"
		mysql_free_result(res);
		syslog(LOG_INFO,"Пользователь %s подключился\n", from);
		return 0;
	}	
}

unsigned short Is_msg_delivered(MYSQL *mysql, char *msg_id, char *tto) {	
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	MYSQL_RES *res;
	
	snprintf(sql, sizeof(sql), "SELECT * FROM delivered WHERE message_id = '%s' \
		and tto IN ((SELECT user_id FROM users WHERE username = '%s'))", msg_id, tto);
		
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);
		fprintf(stderr, "Is_msg_delivered: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Is_msg_delivered: %s\n", mysql_error(mysql));
        return -1;
	}
	
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	if (mysql_num_rows(res) == 0) {
		mysql_free_result(res);
		return FALSE;
	}
	else {
		mysql_free_result(res);	 
		return TRUE;
	}
}

unsigned short Add_to_delivered(MYSQL *mysql, char *msg_id, char *tto) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
		
	snprintf(sql, sizeof(sql), "INSERT INTO delivered(message_id, tto, fromm, time, shown) \
		SELECT message_id, tto, fromm,  NOW(), garanty from messages WHERE message_id = '%s' \
		and tto IN (SELECT user_id from users WHERE username = '%s')", msg_id, tto);
		
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);
		fprintf(stderr, "Add_to_delivered: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Add_to_delivered: %s\n", mysql_error(mysql));
		return -1;
	}
	pthread_mutex_unlock(&mutex);
	return 0;
}

MYSQL_RES *Mysql_find_delivered(MYSQL *mysql, char *from) {
	MYSQL_RES *res;
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	
	snprintf(sql, sizeof(sql), "SELECT  m.message, \
		(SELECT uu.username FROM delivered AS dd, users AS uu WHERE dd.message_id = m.message_id AND dd.tto = uu.user_id) AS tto, \
		(SELECT d.time FROM delivered AS d, users AS u WHERE d.message_id = m.message_id AND d.tto = u.user_id ) AS time, \
		(SELECT de.shown FROM delivered AS de, users AS u WHERE de.message_id = m.message_id AND de.tto = u.user_id ) AS shown, \
		(SELECT del.delivered_id FROM delivered AS del, users AS u WHERE del.message_id = m.message_id AND del.tto = u.user_id ) AS id \
		from messages AS m WHERE message_id IN (SELECT message_id from delivered WHERE fromm IN (SELECT user_id from users \
		WHERE username = '%s'))", from);
	pthread_mutex_lock(&mutex);	
	if (mysql_query(mysql, sql)) {
		fprintf(stderr, "Mysql_find_delivered: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Mysql_find_delivered: %s\n", mysql_error(mysql));
		exit(1);
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	return res;
}

MYSQL_RES *Mysql_find_groups(MYSQL *mysql) {
	MYSQL_RES *res;
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN] = "SELECT groupname FROM groups";
	
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		fprintf(stderr, "Mysql_find_groups: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Mysql_find_groups: %s\n", mysql_error(mysql));
		exit(1);
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	return res;
}		



unsigned short Add_to_group(MYSQL *mysql, char *from, char *groupname) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	MYSQL_RES *res;
	
	snprintf(sql, sizeof(sql), "SELECT * FROM user_group WHERE user_id IN \
	(SELECT user_id FROM users WHERE username = '%s') \
	AND group_id IN (SELECT group_id FROM groups WHERE groupname = '%s')", from, groupname);
	
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		fprintf(stderr, "Mysql_find_delivered: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Mysql_find_groups: %s\n", mysql_error(mysql));
		return -1;
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	
	if (mysql_num_rows(res) == 0) {	
		mysql_free_result(res);
		snprintf(sql, sizeof(sql), "INSERT INTO user_group(user_id, group_id) \
		SELECT u.user_id, g.group_id FROM users AS u, groups AS g WHERE \
		u.username = '%s' AND g.groupname = '%s'", from, groupname);
		
		pthread_mutex_lock(&mutex);
		if (mysql_query(mysql, sql)) {
			pthread_mutex_unlock(&mutex);
			fprintf(stderr, "Add_to_group: %s\n", mysql_error(mysql));
			syslog(LOG_ERR,"Add_to_group: %s\n", mysql_error(mysql));
			return -1;
		}
		pthread_mutex_unlock(&mutex);
		return TRUE;
	}
	else {
		mysql_free_result(res);
		return FALSE;
	}		
}	

unsigned short Is_group_exists(MYSQL *mysql, char *groupname) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	MYSQL_RES *res;
	groupname[strlen(groupname) - 1] = '\0';
	snprintf(sql, sizeof(sql), "SELECT * FROM groups WHERE group_id IN \
		(SELECT group_id FROM groups WHERE groupname = '%s')", groupname);
	
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);
		fprintf(stderr, "Is_group_exists: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Is_group_exists: %s\n", mysql_error(mysql));
		exit(1);
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	if (mysql_num_rows(res) == 0) {	
		mysql_free_result(res);
		return FALSE;
	}	
	else {
		mysql_free_result(res);
		return TRUE;
	}		
}

MYSQL_RES *Find_users_groups(MYSQL *mysql, char *from) {
	MYSQL_RES *res;
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	
	snprintf(sql, sizeof(sql), "SELECT (SELECT g.groupname FROM groups AS g \
		WHERE g.group_id = ug.group_id) AS groupname FROM mydb.user_group AS ug \
		WHERE user_id IN (SELECT user_id FROM users WHERE username = '%s');", from);
	
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		fprintf(stderr, "Mysql_find_delivered: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Mysql_find_delivered: %s\n", mysql_error(mysql));
		exit(1);
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	return res;	
}

unsigned short Is_user_ingroup(MYSQL *mysql, char *from, char *groupname) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	MYSQL_RES *res;
	groupname[strlen(groupname) - 1] = '\0';
	
	snprintf(sql, sizeof(sql), "SELECT * FROM user_group WHERE user_id IN \
		(SELECT user_id FROM users WHERE username = '%s') AND group_id IN \
		(SELECT group_id FROM groups WHERE groupname = '%s')", from, groupname);
	
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);	
		fprintf(stderr, "Is_user_ingroup: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Is_user_ingroup: %s\n", mysql_error(mysql));
		exit(1);
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	if (mysql_num_rows(res) == 0) {	
		mysql_free_result(res);
		return FALSE;
	}	
	else {
		mysql_free_result(res);
		return TRUE;
	}
}

unsigned short Delete_user_fromgroup(MYSQL *mysql, char *from, char *groupname) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	
	snprintf(sql, sizeof(sql), "DELETE FROM user_group WHERE user_id IN \
		(SELECT user_id FROM users WHERE username = '%s') AND \
		group_id IN (SELECT group_id FROM groups WHERE groupname = '%s')", from, groupname);
	
	pthread_mutex_lock(&mutex);	
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);	
		fprintf(stderr, "Delete_user_fromgroup: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Delete_user_fromgroup: %s\n", mysql_error(mysql));
		exit(1);
	}
	pthread_mutex_unlock(&mutex);
	return 0;	
}	

unsigned short Is_user_registered(MYSQL *mysql, char *to) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	MYSQL_RES *res;
	to[strlen(to) - 1] = '\0';
	
	snprintf(sql, sizeof(sql), "SELECT * FROM users WHERE username = '%s'", to);
	
	pthread_mutex_lock(&mutex);
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);	
		fprintf(stderr, "Is_user_registered: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Is_user_registered: %s\n", mysql_error(mysql));
		exit(1);
	}
	res = mysql_store_result(mysql);
	pthread_mutex_unlock(&mutex);
	if (mysql_num_rows(res) == 0) {	
		mysql_free_result(res);
		return FALSE;
	}	
	else {
		mysql_free_result(res);
		return TRUE;
	}
}	

MYSQL_RES *Get_users_fromgroup(MYSQL *mysql, char *groupname) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	snprintf(sql, sizeof(sql), "SELECT (SELECT u.username FROM users AS u \
		WHERE u.user_id = ug.user_id) FROM user_group AS ug WHERE group_id IN \
		(SELECT group_id FROM groups WHERE groupname = '%s')", groupname);
	
	if (mysql_query(mysql, sql)) {
		fprintf(stderr, "Get_users_fromgroup: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Get_users_fromgroup: %s\n", mysql_error(mysql));
		exit(1);
	}
	return (mysql_store_result(mysql));	
}	

unsigned short Create_and_join(MYSQL *mysql, char *groupname, char *from) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	
	snprintf(sql, sizeof(sql), "INSERT  INTO groups (groupname) values ('%s')", groupname);
	pthread_mutex_lock(&mutex);	
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);	
		fprintf(stderr, "Create_and_join: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Create_and_join: %s\n", mysql_error(mysql));
		return -1;
	}
	
	snprintf(sql, sizeof(sql), "INSERT INTO user_group (group_id, user_id) \
		SELECT g.group_id, u.user_id FROM groups AS g, \
		users AS u WHERE g.groupname = '%s' AND u.username = '%s';", groupname, from);
		

	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);	
		fprintf(stderr, "create_and_Join: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"create_and_Join: %s\n", mysql_error(mysql));
		return -1;
	}
	pthread_mutex_unlock(&mutex);		 
	return 1;
}

unsigned short Mark_as_shown(MYSQL *mysql, char *id) {
	char sql[MSG_LEN + MAX_SQL_QUERY_LEN];
	snprintf(sql, sizeof(sql), "UPDATE delivered SET shown = 2 WHERE delivered_id = '%s'", id);
	pthread_mutex_lock(&mutex);			
	if (mysql_query(mysql, sql)) {
		pthread_mutex_unlock(&mutex);	
		fprintf(stderr, "Mark_as_shown: %s\n", mysql_error(mysql));
		syslog(LOG_ERR,"Mark_as_shown: %s\n", mysql_error(mysql));
		return -1;
	}
	pthread_mutex_unlock(&mutex);		 
	return 1;
}

void *ask_for_delivered(void *msg) {
	msg_type *message = (msg_type *)msg;
	char from[MAX_NAME];
	int received, sock = message->todo;
	snprintf(from, sizeof(from), "%s", message->from);
	while (1) {
		sleep(5);
		pthread_mutex_lock(&mutex);
		message->todo = JUST_ASK;
		message->garanty = FALSE;
		snprintf(message->from, sizeof(from), "%s", from);
		sock = sock;
		sendall(sock, (const char*)message, sizeof(msg_type));
		//send(sock, &message, sizeof(message), 0);
		if (Readable_timeo(sock, 5) == 0) 
			fprintf(stderr, "socket timeout\n");
			
		else {
			received = 0;
			while ( ( received = recv(sock, message, sizeof(msg_type), 0))) {
				if (received != sizeof(msg_type) && received != 0){
					printf("recv error from thr\n");
				}
				if (message->todo == FALSE) {
					break;
				}		
				else if (message->todo == TRUE) {
					printf("Уведомление от сервера:\nПользователь %s получил ваше сообщение %s\n\n", message->to, message->msg);
					printf("Выберите действие:\n");
				}
			}
		}	
		pthread_mutex_unlock(&mutex);
	}
}

int Readable_timeo(int fd, int sec) {
	fd_set set;
	struct timeval tv;
	FD_ZERO(&set);
	FD_SET(fd, &set);
	tv.tv_sec = sec;
	tv.tv_usec = 0;
	return (select(fd + 1, &set, NULL, NULL, &tv));
 }



