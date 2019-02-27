#include "mylib.h"

//pthread_mutex_t mutex;

int main(int argc, char *argv[]) { 
	
	data_type data;
	pthread_t tid;
	char ip[16];
	char loglvl[2];
	if (argc < 4 || argc > 5){
		syslog(LOG_ERR,"usage: ./srv <interface IP> <logmask> <user per thr>\n");
		exit(1);
	}
	if (argc == 5) {		
		FILE *options;
		options = fopen (argv[4], "r");
		printf("Обнаружен файл настроек. Параметры заменены на следующие\n");
		while (fscanf (options, "%s%s", ip,  loglvl) != EOF) {
			printf("ip: %s\t loglvl: %s\n", ip, loglvl);
		}
		fclose (options);
		Logmask(loglvl);		
		init((void *)&data, ip, argv[3]);
	}
	else {
		Logmask(argv[2]);		
		init((void *)&data, argv[1], argv[3]);
	}
	
	
	syslog(LOG_NOTICE,"Server started");
		
    if(bind(data.listener, (SA *)&data.serveraddr, sizeof(data.serveraddr)) < 0) {
        perror("bind");
        syslog(LOG_ERR,"bind error");
        exit(2);
    }
    
	if(listen (data.listener, 5) < 0) {
		perror("listen");
		syslog(LOG_ERR,"listen error");
		exit(3);
	}
	
	syslog(LOG_NOTICE,"Server listening on port %d\n", ntohs(data.serveraddr.sin_port));
	
	data.fds[0].fd = data.listener;
	data.fds[0].events = POLLIN;
	data.thrs = 1;
	
	///thread for sigs
	if(pthread_create(&tid, NULL, sig, (void *)&data) != 0) {
						perror("pthread_create");
						syslog(LOG_ERR,"pthread_create");
						exit(4);
	}
	if(pthread_detach(tid) != 0) {
						perror("pthread_detach");
						syslog(LOG_ERR,"pthread_create");
						exit(5);
					}
	
	///thread for accepts
	if(pthread_create(&data.tid, NULL, poll_connection, (void *)&data) != 0) {
						perror("pthread_create");
						syslog(LOG_ERR,"pthread_create");
						exit(4);
	}
	if(pthread_join(data.tid, NULL) != 0) {
						perror("pthread_join");
						syslog(LOG_ERR,"pthread_join");
						exit(5);
	}	
	mysql_close(data.mysql);
	return 0;
}
