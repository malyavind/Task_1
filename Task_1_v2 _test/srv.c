#include "mylib.h"

int main(int argc, char *argv[]) { 
	FILE *options;
	pthread_t tid;
	data_type data;
	db_login_type db;
	int i;
	char ip[IP_SIZE];
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
	
	///getting data for db
	options = fopen ("db_login.txt", "r");																	
	while (fscanf (options, "%s%s%s%s%d", db.ip, db.username, db.password, db.name, &db.port) != EOF) {
	}
	fclose (options);

	///if we have file with options in args
	if (argc == 5) {
		options = fopen (argv[4], "r");
		printf("Обнаружен файл настроек. Параметры заменены на следующие\n");
		while (fscanf (options, "%s%s", ip,  loglvl) != EOF) {
			printf("ip: %s\t loglvl: %s\n", ip, loglvl);
		}
		fclose (options);
		Logmask(loglvl);									///parameter validation and initialization
		init((void *)&data, (void *)&db, ip, argv[3]);
	}
	else {
		Logmask(argv[2]);									///parameter validation and initialization
		init((void *)&data, (void *)&db, argv[1], argv[3]);
	}
			
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
	syslog(LOG_NOTICE,"Server started");
	syslog(LOG_NOTICE,"Server listening on port %d", ntohs(data.serveraddr.sin_port));
	syslog(LOG_NOTICE,"Type q! for exit");

	data.fds[0].fd = data.listener;
	data.fds[0].events = POLLIN;
	data.thrs = 1;
	for (i = 1; i < MAX_CL; i++)
		data.fds[i].fd = -1;
		
	if(pthread_create(&tid, NULL, ext, 0) != 0) {
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
