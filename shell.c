#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>

#define PATH_MAX 4096

int prompt(char* prompt, char* buffer, size_t size);
void parseArgs(int argc, char** argv);
void parseCmd(char** cmd);
char **str_split(char* a_str, const char a_delim);
char *trimwhitespace(char *str);

char* promptText = "308sh> ";

int main(int argc, char** argv) {
	
	if (argc >= 2) parseArgs(argc, argv);

	while (1) {
		char in[100];
		prompt(promptText, in, sizeof(in));
		parseCmd(str_split(in, ' '));
	}
}

//returns 0 if totes okay, 1 if the user input nothing, 2 if the user input was too long
int prompt(char* prompt, char* buffer, size_t size){
	int ch;
	int toolong;
	if (prompt != NULL) {
		printf ("%s", prompt);
		fflush (stdout);
	}
	if (fgets(buffer, size, stdin) == NULL) {
		return 1;
	}

	if (buffer[strlen(buffer) - 1] != '\n') {
		toolong = 0;
		while(((ch = getchar()) != '\n') && (ch != EOF)) {
			toolong = 1;
		}
		return (toolong) ? 2 : 0;
	}
	
	buffer[strlen(buffer) - 1] = '\0';
	return 0;
}

void parseCmd(char** cmds) {
	char* cmd = trimwhitespace(cmds[0]);

	//exit
	if (strcmp(cmd, "exit") == 0) exit(0);

	//pid
	if (strcmp(cmd, "pid") == 0) {
		int pid = getpid();
		printf ("Shell pid: %d\n", pid);
		fflush (stdout);
		return;
	}

	//ppid
	if (strcmp(cmd, "ppid") == 0) {
		int ppid = getppid();
		printf ("Shell parent pid: %d\n", ppid);
		fflush (stdout);
		return;
	}

	//pwd
	if (strcmp(cmd, "pwd") == 0) {
		char currentDir[PATH_MAX];
		getcwd(currentDir, PATH_MAX);
		printf ("%s\n", currentDir);
		fflush (stdout);
		return;
	}

	//cd
	if (strcmp(cmd, "cd") == 0) {
		if (cmds[1] == NULL) chdir(getenv("HOME"));
		else if (chdir(cmds[1]) != 0) perror("cd failed");
		return;
	}

	printf ("Command not found: %s\n", cmd);
	fflush (stdout);
}

void parseArgs(int argc, char** argv) {
	int i = 0;
	for (i = 0; i < argc; i++) {
		char* arg = argv[i];
		if (arg[0] == '-') {
			switch (arg[1]) {
				case 'p': {
					promptText = argv[++i];
				}
			}
		}
	}
}

char* trimwhitespace(char* str) {
	char* end;

	// Trim leading space
	while (isspace((unsigned char)*str))
		str++;

	if (*str == 0) // All spaces?
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	// Write new null terminator character
	end[1] = '\0';

	return str;
}

char **str_split(char* a_str, const char a_delim) {
	char **result = 0;
	size_t count = 0;
	char *tmp = a_str;
	char *last_comma = 0;
	char delim[2];
	delim[0] = a_delim;
	delim[1] = 0;

	while (*tmp)
	{
		if (a_delim == *tmp)
		{
			count++;
			last_comma = tmp;
		}
		tmp++;
	}

	count += last_comma < (a_str + strlen(a_str) - 1);

	count++;

	result = malloc(sizeof(char* ) * count);

	if (result)
	{
		size_t idx = 0;
		char *token = strtok(a_str, delim);

		while (token)
		{
			//assert(idx < count);
			*(result + idx++) = strdup(token);
			token = strtok(0, delim);
		}
		//assert(idx == count - 1);
		*(result + idx) = 0;
	}

	return result;
}