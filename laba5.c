#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <wait.h>
#include <string.h>
#include <time.h>

int sem_k=2;
int sem, msgqid;
int fd, pd[2];
int j, k=0, flag=1, wr;
struct sembuf semwait, semchange[2];
struct msgbuf msgone;
char buf[256],ch;

struct msgbuf   			//Структура сообщения
	{
		long mtype; 		//Тип сообщения
		char mtext[256];	//Текст сообщения
	};

int main(int argc, char *argv[])
{
	if (argc == 1) wr = 1;
	else wr = 0;
	
	//Создание семафоров и получение доступа к набору семафоров
	sem = semget(IPC_PRIVATE, sem_k, 0666 | IPC_CREAT);
	if(sem == -1)
	{
		fprintf(stderr, "Semaphore create error\n");
		return -1;
	}
	
	
	int i;
	if(semctl (sem, 0, SETVAL, 0) == -1)	//Устаналиваем (SETVAL) значение отдельного (0) семафора
	{
		fprintf(stderr, "Semaphore0 set error\n");
		return -1;
	}
	
	for (i = 1; i < sem_k; i++)
	{
		if(semctl (sem, i, SETVAL, 1) == -1)
		{
			fprintf(stderr, "Semaphore%d set error\n",i);
			return -1;
		}
	}
	
	//Создание очереди
	msgqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
	if(msgqid == -1)
	{
		fprintf(stderr, "Queue create error\n");
		return -1;
	}
	
	//Открытие файла
	if ((fd = open("poetry.txt", O_RDONLY)) == -1)
	{
		fprintf(stderr, "poetry.txt open error\n");
		return -1;
	}
	
	//Cоздание процессов
	for (i=0; i < sem_k; i++)
	{
		pd[i] = fork();
		if (pd[i] == -1)
		{
			fprintf(stderr, "Fork error (P0 to P%d)\n",i);
			
			//Отправление сигнала об окончании
			int k;
			for (k = 0; k < i; k++)
			{
				for (j = 0; j < 10; j++) 
				{
				if (kill(pd[k], SIGTERM) != -1)
				break;
				}
				if (j == 10) 
				{
					fprintf(stderr, "Signal send to P%d error \n",k);
					fprintf(stderr, "Uncorrect program ending");
				}
			}
			return -1;
		}
		if (pd[i] == 0)
		{
			if (wr == 1) printf("P0 is making P%d\n",i);
			child(i,fd,sem,msgqid);
		}
	}
	printf("\n");
	
	//Ожидание окончания процесса
	for(i = 0; i < sem_k; i++)
		wait(&pd[i]);
	
	//Удаление семафора
	if(semctl (sem, 0, IPC_RMID) == -1)
	{
		fprintf(stderr, "Semaphore delete error\n");
		return -1;
	}
	
	//Чтение очереди
	printf("Reading queue:");
	while(msgrcv(msgqid, &msgone, 256, 1, IPC_NOWAIT)!=-1)
		printf(msgone.mtext);
	
	//Удаление очереди
	if(msgctl (msgqid, 0, IPC_RMID) == -1)
	{
		fprintf(stderr, "Queue delete error\n");
		return -1;
	}
	close(fd);
	printf("\nP0 finished\n");
	return 0;
}
	
int child(id, fd, sem, msgqid)
{
	int num_str = 0;
	do
	{
		semwait.sem_num = id; 							//Текущее описание процесса
		semwait.sem_op = 0; 								//Ожидание значения 0
		semwait.sem_flg = SEM_UNDO; 					//Установка предыдущему значению sem
		
		semchange[0].sem_num = id;
		semchange[0].sem_op = 1; 						//Добавление 1 в текущий семафор процесса
		semchange[1].sem_num = (id + 1) % sem_k;	//Номер семафора следующего процесса
		semchange[1].sem_op = -1; 						//Вычитание 1 из семафора следующего процесса
		
		//Ожидание 0 значения
		if ((semop(sem, &semwait, 1)))
		{
			fprintf(stderr, "Semaphore%d wait error\n",id);
			exit(-1);
		}
	
		//Задержка времени
		int time;
		for (time = 0; time < rand() % 1000000 + 10000000; ++time);
		
		//Чтение из файла 4 строки
		do
		{
			for(k=0; k<256; k++) buf[k]=0;
			k=0;
			do
			{
				flag = read(fd, &ch, sizeof(char));
				buf[k] = ch;
				k++;
			} while (flag == 1  && ch != '\n');
			
			if (buf[0] != '\n')
			{
				sprintf(msgone.mtext, buf);
				msgone.mtype=1;
				printf("Test: %.*s\n", k + 1, buf);
				//Добавление в очередь
				if (msgsnd(msgqid, (void*)&msgone, 256, 0) == -1)
				{
					fprintf(stderr, "Message add error by process%d\n",id);
					exit(-1);
				}
			}
			num_str++;
		} while(num_str < 4 && flag == 1);
		
	//Изменяем текущее и следующее значение семафора
	//if(num_str >= 4){
	num_str = 0;
	if (semop(sem, semchange, 2))
	{
		fprintf(stderr, "Semaphore change error %d\n",id);
		exit(-1);
	}
	//}
	if (wr==1) printf("Read&added by P%d\n",id);
	
	} while(flag==1);
	
	exit(0);
}


