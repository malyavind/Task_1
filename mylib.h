#ifndef MY_LIB_H
#define MY_LIB_H

#define TIME_SIZE 20
#define MAX_NAME 32
#define PORT 3325
#define MAX_CL 1024
#define CL_PER_THR 2
#define MSG_LEN 255  ///because of max characters in database cell
#define MAX_SQL_QUERY_LEN 512
#define NO_MESSAGES 13
#define NEW 14
#define MAX_DELAY 71582788 ///because of unsigned max value / 60
#define TRUE 1
#define FALSE 0

#define SA struct sockaddr

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <mysql/mysql.h>
#include <time.h>
#include <syslog.h>

#pragma pack(push,1)
typedef struct {
	char id[11];
	unsigned delay;
	unsigned short todo, new_msg;
	unsigned short garanty;	
	char from[MAX_NAME], to[MAX_NAME];
	char msg[MSG_LEN];
	char time[TIME_SIZE];

} msg_type;
#pragma pack(pop)

typedef struct {
	int listener;
	int accepter[MAX_CL];
	int clients;
	int protocol;
	int addr;
	int thrs;
	int users_per_thread;
	
	pthread_t tid;
	
	struct pollfd fds[MAX_CL];
	struct sockaddr_in serveraddr;
	
	MYSQL *mysql;
	
} data_type;

void* poll_connection (void *args);
void init(void *args, char *ip, char *upt);
void clean_stdin(void);
void* sig(void *args);
void Print_menu();
void Logmask(char *logmask);

int Mysql_check_user(MYSQL *mysql, char *from);
int menu();

unsigned short Mysql_insert_msg(MYSQL *mysql, char *message, char *to, char *from, int delay);
unsigned short Is_msg_delivered(MYSQL *mysql, char *msg_id, char *tto);
unsigned short Add_to_group(MYSQL *mysql, char *from, char *groupname);
unsigned short Is_group_exists(MYSQL *mysql, char *groupname);
unsigned short Is_user_ingroup(MYSQL *mysql, char *from, char *groupname);
unsigned short Delete_user_fromgroup(MYSQL *mysql, char *from, char *groupname);
unsigned short Add_to_delivered(MYSQL *mysql, char *delivered, char *tto);
unsigned short Is_user_registered(MYSQL *mysql, char *to);
unsigned short Create_and_join(MYSQL *mysql, char* groupname, char *from);

MYSQL_RES *User_by_name(MYSQL *mysql, char *name);
MYSQL_RES *Mysql_find_msg(MYSQL *mysql, char *to);
MYSQL_RES *Mysql_find_delivered(MYSQL *mysql, char *from);
MYSQL_RES *Mysql_find_groups(MYSQL *mysql);
MYSQL_RES *Find_users_groups(MYSQL *mysql, char *from);
MYSQL_RES *Get_users_fromgroup(MYSQL *mysql, char *togroup);



#endif	
