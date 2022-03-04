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
int prompt(char* prompt, char* in, size_t size);
void parseArgs(int argc, char** argv);
void parseCmd(char** cmd, int len);
int splitStringProtected(char* in, char* out[], size_t size);
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
		char in[100]; //100 is an arbitrary limit but it is enforced
		prompt(promptText, in, sizeof(in)); //prompt the user
		checkBackground(); //check for completed background tasks and reap them
		if (*(trimwhitespace(in) + 1) == '\0') continue; //if nothing was entered by the user then prompt again, don't bother processing it
		char* parsed[20]; //again, an arbitrary limit on number of args
		int numParsed = splitStringProtected(in, parsed, 20); //split the string into an array of strings and get how many args there were
		parseCmd(parsed, numParsed); //NOW FOR THE MAIN COURSE
	}
}

//returns 0 if totes okay, 1 if the user input nothing, 2 if the user input was too long
int prompt(char* prompt, char* in, size_t size){
	int ch;
	int toolong;
	if (prompt != NULL) { 
		printf("%s", prompt); //prompt the user
		fflush(stdout);
	}
	if (fgets(in, size, stdin) == NULL) { //wait for user to enter something, if fgets returns null return an error (that i dont use lol)
		return 1;
	}

	if (in[strlen(in) - 1] != '\n') { //if the end of our string from fgets doesnt end in a newline we might have screwed up
		toolong = 0;
		while(((ch = getchar()) != '\n') && (ch != EOF)) { //go through the rest of stdin one character at a time. if we find a newline or endoffile then user input was too long
			toolong = 1;
		}
		return (toolong) ? 2 : 0; //if we didn't find any of the above, uhhhhh i guess it's okay, return and we good
	}

	//in retrospect i was ambitious with this function and didnt use all of its features in main
	
	in[strlen(in) - 1] = '\0'; //add null terminator
	return 0;
}

/**
 * @brief meant for parsing the commands given to it and carrying out thy will
 * 
 * @param cmds a string array of all arguments
 * @param len the number of arguments
 */
void parseCmd(char** cmds, int len) {
	stripQuotesBatch(cmds, len); //some of these might have quotes on em. get rid of those
	char* cmd = trimwhitespace(cmds[0]); //trim whitespace away from the first one. that one is our command

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
		cmds[len - 1] = NULL; //get rid of the &
		executeOtherBG(cmd, cmds);
		return;
	}
	//the other background command case where the & is attached to the command instead of at the end
	//in this case all arguments are discarded (this behavior was modeled after zsh)
	int cmdLen = strlen(cmd);
	if (cmd[cmdLen - 1] == '&') {
		cmd[cmdLen - 1] = '\0'; //get rid of the &
		cmds[1] = NULL; //get rid of the arguments
		executeOtherBG(cmd, cmds);
		return;
	}

	//other command foreground
	if (executeOtherFG(cmd, cmds) < 0) {
		perror("cannot execute");
	}
}

/**
 * @brief handles reaping background processes, managing the list, and printing the notice
 * 
 * @return int: the number of processes that exited and were reaped
 */
int checkBackground() {
	int status;
	int pid;
	int exited;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { //reap all zombified children
		if (WIFEXITED(status)) exited++; //count how many exited
		if (exited) {
			BackgroundProcess* process = findBGProcess(pid, &processList); //find the node for this process
			printExit(process->name, pid, WEXITSTATUS(status)); //print process exit notice
			if (process) removeBGProcess(findBGProcess(pid, &processList), &processList); //if node exists, remove it
		}
	}
	return exited; //not sure what i intended to use this return value for
}

/**
 * @brief frees an array of pointers that were all malloc'd
 * 
 * @param array an array of malloc'd pointers
 * @param len the length of said array
 */
void freePointerArray(void** array, int len) { //I didn't end up using this one but I thought i'd keep it bc hey why not
	int i;
	for (i = 0; i < len; i++) {
		free(array[i]);
	}
}

/**
 * @brief prints an array of strings
 * 
 * @param name the name of the array. will be printed along with it.
 * @param array array of strings. the array must be null terminated
 */
void printStringArray(char* name, char* array[]) {
	int i = 0;

	printf("%s: {", name);

	while(array[i] != NULL) {
		printf("%s, ", array[i++]);
	}

	printf("}\n");
	fflush(stdout);
}

/**
 * @brief for printing all processes in the list
 * 
 * @param list the list to print
 */
void printProcessList(ProcessList* list) {
	printf("Number of background processes: %d\n", list->count); //print the total

	BackgroundProcess* current = list->head; //start at the head
	while (current != NULL) { //go until we run out
		printf("Name: %s, pid: %d\n", current->name, current->pid); //print the node
		fflush(stdout);
		current = current->next; //advance to the next node
	}
}
/**
 * @brief executes in the background
 * 
 * @param cmd command to execute
 * @param args arguents to pass along
 * @return int the pid of the child process
 */
int executeOtherBG(char* cmd, char** args) {
	int pid = executeOther(cmd, args); //execute as standard but don't wait
	addBGProcess(cmd, pid, &processList);
	return pid;
}

/**
 * @brief print the exit notice given process details
 * 
 * @param name process name
 * @param pid pid
 * @param code the process' exit code
 */
void printExit(char* name, pid_t pid, int code) {
	printf("%s (%d) exited with code %d\n", name, pid, code);
	fflush(stdout);
}

/**
 * @brief forks and executes a given command
 * 
 * @param cmd command to execute
 * @param args arguments to pass along
 * @return int the pid of the child
 */
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

/**
 * @brief for executing in the foreground. blocking
 * 
 * @param cmd command to execute
 * @param args arguments to pass along
 * @return int: pid of child
 */
int executeOtherFG(char* cmd, char** args) {
	int status = 1;
	int pid = executeOther(cmd, args); //execute
	if (pid > 0) {
		waitpid(pid, &status, 0); //wait for child to finish
		printExit(cmd, pid, WEXITSTATUS(status)); //print child exit status
	}
	return pid;
}

/**
 * @brief strips quotes from strings in an array
 * 
 * @param strings array of strings
 * @param len length of array
 */
void stripQuotesBatch(char** strings, int len) {
	int i;
	for (i = 0; i < len; i++) {
		stripQuotes(strings[i]); //just run stripQuotes() for each string
	}
}

/**
 * @brief strips quotes from a string
 * 
 * @param string string to strip quotes from
 */
void stripQuotes(char* string) {
	int len = strlen(string);
	if (!isQuoted(string)) return; //if string is not enclosed by quotes, just return
	substring(string, 1, len - 2); //else trim off the beginning and end of the string
}

//both are inclusive
/**
 * @brief trims a string to a piece of itself
 * 
 * @param string string to substring
 * @param first index of first char to keep (inclusive)
 * @param last index of last char to keep (inlusive)
 */
void substring(char* string, int first, int last) {
	string += first;
	string[last + 1] = '\0';
}

/**
 * @brief decides if string is enclosed by double quotes
 * 
 * @param string string to check
 * @return true if string is enclosed by quotes
 * @return false if string is not
 */
bool isQuoted(char* string) {
	int len = strlen(string);
	return (string[0] == '"') && (string[len - 1] == '"');
}

/**
 * @brief parse the arguments the shell was started with
 * 
 * @param argc number of args
 * @param argv arguments to parse
 */
void parseArgs(int argc, char** argv) {
	int i = 0;
	for (i = 0; i < argc; i++) { //go through all arguments
		char* arg = argv[i];
		if (arg[0] == '-') { //check if argument is a - argument
			switch (arg[1]) { 
				case 'p': { //-p is the only one we implement here
					promptText = argv[++i]; //just set prompt text to the next argument 
				}
			}
		}
	}
}

/**
 * @brief removes whitespace from beginning and end of a string
 * 
 * @param string string to trim whitespace from
 * @return char* trimmed string
 */
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

/**
 * @brief adds a background process node to the linked list
 * 
 * @param name process name
 * @param pid process id
 * @param list pointer to the list
 * @return BackgroundProcess* the new node
 */
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

/**
 * @brief finds a background process given a pid
 * 
 * @param pid pid to look for
 * @param list pointer to list to look it
 * @return BackgroundProcess* process found, null if not found
 */
BackgroundProcess* findBGProcess(pid_t pid, ProcessList* list) {
	BackgroundProcess* head = list->head;
	while (head->pid != pid) {
		if (head->next == NULL) return NULL; 
		head = head->next;
	}
	return head;
}

/**
 * @brief removes the given background process from the list
 * 
 * @param process 
 * @param list 
 */
void removeBGProcess(BackgroundProcess* process, ProcessList* list) {
	if (process == list->tail) list->tail = process->prev; //if this is the tail then the tail shall now be the previous node
	if (process == list->head) list->head = process->next; //if this is the head then the head shall now by the next node
	
	//relink the list if this node was in the middle somewhere
	if (process->prev) process->prev->next = process->next; 
	if (process->next) process->next->prev = process->prev;

	free(process->name); //this was malloc'd so free it
	free(process); //as was the node itself. clean up!
	list->count--; //we now have one fewer node so update count to reflect that
}

/**
 * @brief splits a string by spaces but only when NOT enclosed by double quotes
 * she's a big one
 * 
 * @param in the string to split
 * @param out output array of string pointers
 * @param size max number of strings our output array can handle (this includes the last entry which will be null)
 * @return size_t 
 */
int splitStringProtected(char* in, char* out[], size_t size) {
    char* pos; //for storing our current position through the input string
	char* start; //for storing the start of the current word
    int c; //just a container to put the dereferenced position in
    enum states { BASE, IN_WORD, IN_STRING } state = BASE; //either we're in a string, in a word, or neither. we start in neither
    int count = 0; //we want to count arguments to return them

	size--; //the last index must be null so our size is effectively one less for actual data

    for (pos = in; count < size && *pos != '\0'; pos++) { //go until we either hit the end of the input string or we run out of space in our output array
        c = (unsigned char) *pos; //dereference the current position through in and remember it
        switch (state) {
			case BASE: //if we arent currently in a word or a string
				if (isspace(c)) { //we found a space
					continue; //no change. we are still in neither
				}

				if (c == '"') { //we found a quote
					state = IN_STRING; //we are now in a string
					start = pos + 1; //the start of the string is one after the quote
					continue;
				}

				//otherwise, if we found anything else
				state = IN_WORD; //we are now in a word
				start = pos; //the start of the word is where we are
				continue;

			case IN_STRING: //if we are currently in a string
				if (c == '"') { //and we find a quote
					*pos = 0; //the string just ended. replace the quote with a null character
					out[count++] = start; //we've got a new string for the output array (also incremement count)
					state = BASE; //we've gone back to the base state
				}
				continue;

			case IN_WORD: //if we're currently in a word
				if (isspace(c)) { //and we find a space
					*pos = 0; //then the word just ended. replace the space with a null character
					out[count++] = start; //we've got a new string for the output array (also increment count)
					state = BASE; //we've gone back to the base state
				}
				continue;
			}
    }

	//for the last word/string
    if (state != BASE && count < size) //if we were in a word or string when we ended AND we still have space in the output array
        out[count++] = start; //just go ahead and add it

	out[count] = NULL; //the very next input will be null to signal the end of the array

    return count; //return how many we found
}