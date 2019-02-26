# Task_1
Выполненное тестовое задание.

Программа писалась под операционной системой Ubuntu 18.04.2 LTS.

Для хранения сообщений, регистрации пользователей и групп используется база данных MySQL.

При необходимости, MySQL пакеты, требуемые для работы программы можно загрузить с помощью скрипта mysql.sh.

Сборка осуществляется с помощью Makefile.

Сервер  запускается из командной строки с параметрами запуска.

./srv <Interface> <loglvl> <users_per_thread> <opt.txt>
  
interface - адрес сетевого интерфейса сервера (поддержка IPv6 не реализована).

loglvl - уровень логирования (1-3).

users_per_thread - количество пользователей, которое сервер будет обслуживать на одном потоке. При привышении этого количество клиентов создается новый поток.

opt.txt - файл с настройками для работы сервера (интерфейс для запуска, уровень логирования). Использование этого параметра опционально.

Пример: ./srv 192.168.22.18 3 1 opt.txt



Клиент запускается из командной строки следующим образом:

./cli <serverIP> <username>
 
serverIP - IP aдрес сервера.

username - имя отправителя.

Пример: ./cli 192.168.22.18 Dima
  
