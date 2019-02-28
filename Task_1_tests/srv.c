#include "mylib.h"

int main(int argc, char *argv[]) { 
	pthread_t tid;
	data_type data;
	int i;
	char ip[16];
	char loglvl[2];
	///SIGUSR1
	struct sigaction act1;
	memset(&act1, 0, sizeof(act1));
	act1.sa_handler = hdl1;
	act1.sa_flags = SA_RESTART;
	sigfillset(&act1.sa_mask); 
	sigaction(SIGUSR1, &act1, NULL);
	///SIGUSR2
	struct sigaction act2;
	memset(&act2, 0, sizeof(act2));
	act2.sa_handler = hdl2;
	act2.sa_flags = SA_RESTART;
	sigfillset(&act2.sa_mask); 
	sigaction(SIGUSR2, &act2, NULL);
	
	
	if (argc < 4 || argc > 5){
		syslog(LOG_ERR,"usage: ./srv <interface IP> <logmask> <user per thr> (<opt.txt>)\n");
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
	for (i = 1; i < MAX_CL; i++)
		data.fds[i].fd = -1;
		
	if(pthread_create(&tid, NULL, ext, 0/*(void *)&data*/) != 0) {
		perror("pthread_create");
		syslog(LOG_ERR,"pthread_create");
		exit(4);
	}
	if(pthread_detach(tid) != 0) {
						perror("pthread_detach");
						syslog(LOG_ERR,"pthread_detach");
						exit(5);
	}
		
	poll_connection(&data);
		
	mysql_close(mysql);
	return 0;
}
