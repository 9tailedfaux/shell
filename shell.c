#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdbool.h>

#define PATH_MAX 4096

int prompt(char* prompt, char* buffer, size_t size);
void parseArgs(int argc, char** argv);
void parseCmd(char** cmd, int len);
char** splitStringProtected(char* string, const char delimIn, char protecc);
char* trimwhitespace(char *str);
int executeOther(char* cmd, char** args);
void printStringArray(char* name, char* array[]);
char* stripQuotes(char* string);
char** stripQuotesBatch(char** strings, int len);
void freePointerArray(void** array, int len);
bool isQuoted(char* string);
void substring(char* string, int first, int last);

char* promptText = "308sh> ";

int main(int argc, char** argv) {
	
	if (argc >= 2) parseArgs(argc, argv);

	while (1) {
		char in[100];
		prompt(promptText, in, sizeof(in));
		parseCmd(splitStringProtected(in, ' ', '"'), argc);
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

void parseCmd(char** cmds, int len) {
	printStringArray("cmds before", cmds);
	cmds = stripQuotesBatch(cmds, len);
	printStringArray("cmds after", cmds);
	char* cmd = trimwhitespace(cmds[0]);
	

	//exit
	if (strcmp(cmd, "exit") == 0) {
		printf("Exiting...\n");
		exit(0);
	} 

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

	//other command background
	//if (strcmp(cmds[]))

	//other command
	if (executeOther(cmd, cmds) != 0) {
		printf ("Command not found: %s\n", cmd);
		fflush (stdout);
	}
	else {

	}

	freePointerArray((void**) cmds, len);
}

void freePointerArray(void** array, int len) {
	int i;
	for (i = 0; i < len; i++) {
		free(array[i]);
	}
}

//note string array must be null terminated
void printStringArray(char* name, char* array[]) {
	int i = 0;

	printf("%s: {", name);

	while(array[i] != NULL) {
		printf("%s, ", array[i++]);
	}

	printf("}\n");
	fflush(stdout);
}

int executeOther(char* cmd, char** args) {
	int status = 1;
	int pid = fork();
	if (pid == 0) {
		int pid = getpid();
		printf ("Child pid: %d\n", pid);
		exit(execvp(cmd, args));
	}
	else {
		waitpid(pid, &status, 0);
		printf("Child %d exited with status: %d\n", pid, WEXITSTATUS(status));
	}
	return status;
}

char** stripQuotesBatch(char** strings, int len) {
	int i;
	for (i = 0; i < len; i++) {
		strings[i] = stripQuotes(strings[i]);
	}
	return strings;
}

char* stripQuotes(char* string) {
	int len = strlen(string);
	if (!isQuoted(string)) return string;
	char* returnBoi = (char*) malloc(sizeof(char) * (len - 2));
	substring(returnBoi, 1, len - 2);
	return returnBoi;
}

//both are inclusive
void substring(char* string, int first, int last) {
	strncpy(string, &string[first], last - first + 1);
}

bool isQuoted(char* string) {
	int len = strlen(string);
	return (string[0] == '"') && (string[len - 1] == '"');
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

char **splitStringProtected(char* string, const char delimIn, char protecc) {

	char **result = 0;
	int count = 0;
	char *temp = string;
	char *last = 0;
	bool protected = false;

	while (*temp) {
		if (delimIn == *temp) {
			if (*temp == protecc) protected = !protected;
			if (!protected) count++;
			last = temp;
		}
		temp++;
	}

	protected = false;

	count += last < (string + strlen(string) - 1);

	count++;

	result = malloc(sizeof(char* ) * count);

	if (result) {
		int i = 0;
		int x = 0;
		temp = string;
		while (x < count - 1) {
			//printf("%s, x=%d\n", temp, x);
			if (temp[i] == protecc) protected = !protected;
			if (temp[i] == delimIn && !protected) {
				result[x] = temp;
				//issue is with the following line
				substring(result[x], 0, i);
				x++;
				temp += i;
				i = -1;
			}
			i++;
		}
	}

	return result;
}