#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#define debug 0
#define io 1
#define iodebug 0


int execute(char **args, int filein, char* fileout) {
	int i, j;
#if io
	int fdout;
	if (fileout) {
		if (*fileout == '>')
			fdout = open(fileout + 1, O_RDWR | O_CREAT | O_APPEND, 0644);
		else
			fdout = open(fileout, O_RDWR | O_CREAT, 0644);
		close(fileno(stdout));
		dup2(fdout, fileno(stdout));
		close(fdout);
	}
	if (filein != fileno(stdin)) {
		close(fileno(stdin));
		dup2(filein, fileno(stdin));
		close(filein);
	}
#endif
	/* �ڽ����� */
	if (strcmp(args[0], "cd") == 0) {
		if (args[1])
			if (chdir(args[1])) {
				printf("%s: No such file or directory\n", args[1]);
			}
		return 0;
	}
	if (strcmp(args[0], "pwd") == 0) {
		char wd[4096];
		puts(getcwd(wd, 4096));
		return 0;
	}
	/*export���û�������*/
	if (strcmp(args[0], "export") == 0) {
		for (i = 1; args[i]; i++) {
			for (j = 1; *(args[i] + j) != '='; j++)
				;
			if (*(args[i] + j) == '=')
				*(args[i] + j) = '\0';
			else
				return 255;
			setenv(args[i], args[i] + j + 1, 1);
		}
		return 0;
	}


	/* �ⲿ���� */
	pid_t pid = fork();
	if (pid == 0) {
		/* �ӽ��� */
		execvp(args[0], args);
		/* execvpʧ�� */
		return 255;
	}
	/* ������ */
	waitpid(pid, NULL, 0);
	return 0;
}
void do_pipe(char **args, int pipenum, int filein, char* fileout) {
	int fd[2];
	int i;
#if io
	int fdout;
#endif
	for (i = 0; args[i] && *args[i] != '|'; i++)
		;
	if (args[i] == NULL || args[i + 1] == NULL)
		return;
	args[i] = NULL;
	pipe(fd);
	pid_t pid1, pid2;
	if ((pid1 = fork()) != 0) {//������
		if ((pid2 = fork()) == 0) {//�ӽ���
			waitpid(pid1, NULL, 0);
			close(fd[1]);
			close(fileno(stdin));
			dup2(fd[0], fileno(stdin));
			if (pipenum > 1) {
#if debug
				printf("pipein %d\n", getpid());
#endif
				do_pipe(args + i + 1, pipenum - 1, fd[0], fileout);
				close(fd[0]);
				exit(0);
			}
			else {
				close(fd[0]);
#if debug
				printf("last pipe %d\n", getpid());
#endif
#if io
				if (fileout) {
					if (*fileout == '>')
						fdout = open(fileout + 1, O_RDWR | O_CREAT | O_APPEND, 0644);
					else
						fdout = open(fileout, O_RDWR | O_CREAT, 0644);
					close(fileno(stdout));
					dup2(fdout, fileno(stdout));
					close(fdout);
				}
#endif
				execvp(args[i + 1], args + i + 1);
			}
			return;
		}
		else {
			close(fd[0]);
			close(fd[1]);
			waitpid(pid2, NULL, 0);
		}
	}
	/*�����׸�����*/
	else {
#if debug
		printf("frontcmd%d %d\n", pipenum, getpid());
#endif
		close(fileno(stdin));
		dup2(filein, fileno(stdin));
		close(filein);
		close(fd[0]);
		close(fileno(stdout));
		dup2(fd[1], fileno(stdout));
		close(fd[1]);
		execvp(args[0], args);
	}
}

int main() {
	/* ����������� */
	char cmd[256];
	/* �����в��ɵĸ����֣��Կ�ָ���β */
	char *args[128];
	/*�ܵ����������*/
	int pipenum;
	int nextloop;
	int filein;
	int i, j;
	char* fileout;
	int sdout, sdin;
	while (1) {
		/*�����׼�������*/
		sdout = dup(fileno(stdout));
		sdin = dup(fileno(stdin));
		/* ��ʾ�� */
		printf("# ");
		fflush(stdin);
		pipenum = 0;
		nextloop = 0;
		filein = fileno(stdin);
		fileout = NULL;
		fgets(cmd, 256, stdin);
		/* �����β�Ļ��з� */
		for (i = 0; cmd[i] != '\n'; i++)
			;
		cmd[i] = '\0';
		/* ��������� */
		args[0] = cmd;
		for (i = 0; *args[i]; i++)
			for (args[i + 1] = args[i] + 1; *args[i + 1]; args[i + 1]++) {
				/*���������*/
				if (*args[i + 1] == ' ') {
					*args[i + 1] = '\0';
					args[i + 1]++;
					for (; *args[i + 1] == ' '; args[i + 1]++)
						;
					break;
				}
			}
		args[i] = NULL;
		/*ʶ���������*/
		for (i = 0; args[i]; i++) {
			/*pipe*/
			if (strcmp(args[i], "|") == 0) {
				pipenum++;
				continue;
			}
			/*��������ض���*/
#if io
			if (*args[i] == '<') {
				filein = open(args[i] + 1, O_RDWR);
				if (filein == -1) {
					printf("%s: No such file or directory\n", args[i] + 1);
					filein = fileno(stdin);
					nextloop = 1;
				}
				for (j = i; args[j + 1]; j++)
					args[j] = args[j + 1];
				args[j] = NULL;
#if iodebug
				printf("in\n");
#endif
				i--;
				continue;
			}
			if (*args[i] == '>') {
				fileout = args[i] + 1;
				for (j = i; args[j + 1]; j++)
					args[j] = args[j + 1];
				args[j] = NULL;
#if iodebug
				printf("out\n");
#endif
				i--;
				continue;
			}
#endif
			if (*args[i] == '$')
				args[i] = getenv(args[i] + 1);
		}
		if (nextloop) {
			nextloop = 0;
			continue;
		}
		/* û���������� */
		if (!args[0])
			continue;
		if (strcmp(args[0], "exit") == 0)
			return 0;
		if (pipenum == 0) {
			if (execute(args, filein, fileout) == 255)
				return 255;
		}
		else
		{
#if debug
			printf("start!\n");
#endif
			do_pipe(args, pipenum, filein, fileout);
#if debug
			printf("over\n");
#endif
		}
		wait(NULL);
		/*�ָ���׼�������*/
		dup2(sdout, fileno(stdout));
		dup2(sdin, fileno(stdin));
	}
}