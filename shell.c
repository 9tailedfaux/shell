#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdbool.h>

#define PATH_MAX 4096

/**
 * @brief this struct represents a background process.
 * It is to be added when a background process is executed and removed when the process is reaped.
 * It follows a fairly standard doubly linked list implementation.
 * 
 */
typedef struct _BackgroundProcess {
	struct _BackgroundProcess* prev;
	struct _BackgroundProcess* next;
	char* name;
	int pid;
} BackgroundProcess;

/**
 * @brief The manager for the background process list.
 * This struct maintains the head and tail of the list, as well as how many nodes it contains.
 * 
 */
typedef struct _ProcessList {
	BackgroundProcess* head;
	BackgroundProcess* tail;
	int count;
} ProcessList;

//function declarations. definitions come later :)
int prompt(char* prompt, char* buffer, size_t size);
void parseArgs(int argc, char** argv);
void parseCmd(char** cmd, int len);
size_t splitStringProtected(char* buffer, char* argv[], size_t argv_size);
char* trimwhitespace(char *string);
int executeOtherFG(char* cmd, char** args);
void printStringArray(char* name, char* array[]);
void stripQuotes(char* string);
void stripQuotesBatch(char** strings, int len);
void freePointerArray(void** array, int len);
bool isQuoted(char* string);
void substring(char* string, int first, int last);
int checkBackground();
int executeOther(char* cmd, char** args);
void printProcessList(ProcessList* list);
void removeBGProcess(BackgroundProcess* process, ProcessList* list);
BackgroundProcess* findBGProcess(pid_t pid, ProcessList* list);
BackgroundProcess* addBGProcess(char* name, int pid, ProcessList* list);
int executeOtherBG(char* cmd, char** args);
void printExit(char* name, pid_t pid, int code);

char* promptText = "308sh> "; //this stores the prompt text. it has a default value but may be changed
ProcessList processList; 

int main(int argc, char** argv) {
	
	if (argc >= 2) parseArgs(argc, argv); //if we only have one argument then it's just the program's name so not worth parsing

	while (1) {
		char in[100];
		prompt(promptText, in, sizeof(in));
		checkBackground();
		if (*(trimwhitespace(in) + 1) == '\0') continue;
		char* parsed[20];
		int numParsed = splitStringProtected(in, parsed, 20);
		parseCmd(parsed, numParsed);
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
	stripQuotesBatch(cmds, len);
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

	//jobs
	if (strcmp(cmd, "jobs") == 0) {
		printProcessList(&processList);
		return;
	}

	//other command background
	if ((strcmp(cmds[len - 1], "&") == 0) && len > 1) {
		cmds[len - 1] = NULL;
		executeOtherBG(cmd, cmds);
		return;
	}
	int cmdLen = strlen(cmd);
	if (cmd[cmdLen - 1] == '&') {
		cmd[cmdLen - 1] = '\0';
		cmds[1] = NULL;
		executeOtherBG(cmd, cmds);
		return;
	}

	//other command foreground
	if (executeOtherFG(cmd, cmds) < 0) {
		perror("cannot execute");
	}
}

int checkBackground() {
	int status;
	int pid;
	int exited;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		if (WIFEXITED(status)) exited++;
		if (exited) {
			BackgroundProcess* process = findBGProcess(pid, &processList);
			printExit(process->name, pid, WEXITSTATUS(status));
			if (process) removeBGProcess(findBGProcess(pid, &processList), &processList);
		}
	}
	return exited;
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

void printProcessList(ProcessList* list) {
	printf("Number of background processes: %d\n", list->count);

	BackgroundProcess* current = list->head;
	while (current != NULL) {
		printf("Name: %s, pid: %d\n", current->name, current->pid);
		fflush(stdout);
		current = current->next;
	}
}

int executeOtherBG(char* cmd, char** args) {
	int pid = executeOther(cmd, args);
	addBGProcess(cmd, pid, &processList);
	return pid;
}

void printExit(char* name, pid_t pid, int code) {
	printf("%s (%d) exited with code %d\n", name, pid, code);
	fflush(stdout);
}

int executeOther(char* cmd, char** args) {
	int pid = fork();
	if (pid > 0) {
		printf ("Executing %s (%d)\n", cmd, pid);
	}
	if (pid == 0) {
		fflush(stdout);
		exit(execvp(cmd, args));
	}
	return pid;
}

int executeOtherFG(char* cmd, char** args) {
	int status = 1;
	int pid = executeOther(cmd, args);
	if (pid > 0) {
		waitpid(pid, &status, 0);
		printExit(cmd, pid, WEXITSTATUS(status));
	}
	return pid;
}

void stripQuotesBatch(char** strings, int len) {
	int i;
	for (i = 0; i < len; i++) {
		stripQuotes(strings[i]);
	}
}

void stripQuotes(char* string) {
	int len = strlen(string);
	if (!isQuoted(string)) return;
	substring(string, 1, len - 2);
}

//both are inclusive
void substring(char* string, int first, int last) {
	string += first;
	string[last + 1] = '\0';
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

char* trimwhitespace(char* string) {
	char* end;

	// Trim leading space
	while (isspace((unsigned char)*string)) //pushes string's pointer forward until it finds something that isnt a space
		string++;

	if (*string == 0) // if we encounter a null character then string is literally just spaces so just return is
		return string;

	// Trim trailing space
	end = string + strlen(string) - 1;
	while (end > string && isspace((unsigned char)*end)) //same as before but backwards this time. move the endpoint back until we find a non-space character
		end--;

	// Write new null terminator character
	end[1] = '\0';

	return string;
}

BackgroundProcess* addBGProcess(char* name, int pid, ProcessList* list) {
	BackgroundProcess* tail = list->tail;
	BackgroundProcess* new = malloc(sizeof(BackgroundProcess));
	new->name = malloc((sizeof(char) * strlen(name)) + 1);
	strcpy(new->name, name);
	new->pid = pid;
	new->prev = tail;
	new->next = NULL;
	if (tail) tail->next = new;
	else {
		list->head = new;
	}
	list->tail = new;
	list->count++;
	return new;
}

//returns NULL if not found
BackgroundProcess* findBGProcess(pid_t pid, ProcessList* list) {
	BackgroundProcess* head = list->head;
	while (head->pid != pid) {
		if (head->next == NULL) return NULL; 
		head = head->next;
	}
	return head;
}

void removeBGProcess(BackgroundProcess* process, ProcessList* list) {
	if (process == list->tail) list->tail = process->prev;
	if (process == list->head) list->head = process->next;
	if (process->prev) process->prev->next = process->next;
	if (process->next) process->next->prev = process->prev;
	free(process->name);
	free(process);
	list->count--;
}

size_t splitStringProtected(char* buffer, char* argv[], size_t argv_size) {
    char* p;
	char* start_of_word;
    int c;
    enum states { BASE, IN_WORD, IN_STRING } state = BASE;
    size_t argc = 0;

	argv_size--;

    for (p = buffer; argc < argv_size && *p != '\0'; p++) {
        c = (unsigned char) *p;
        switch (state) {
        case BASE:
            if (isspace(c)) {
                continue;
            }

            if (c == '"') {
                state = IN_STRING;
                start_of_word = p + 1; 
                continue;
            }
            state = IN_WORD;
            start_of_word = p;
            continue;

        case IN_STRING:
            if (c == '"') {
                *p = 0;
                argv[argc++] = start_of_word;
                state = BASE;
            }
            continue;

        case IN_WORD:
            if (isspace(c)) {
                *p = 0;
                argv[argc++] = start_of_word;
                state = BASE;
            }
            continue;
        }
    }

    if (state != BASE && argc < argv_size)
        argv[argc++] = start_of_word;

	argv[argc] = NULL;

    return argc;
}