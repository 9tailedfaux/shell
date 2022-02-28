#include <stdio.h>
#include <string.h>

int prompt(char* prompt, char* buffer, size_t size);
void parseArgs(int argc, char** argv);

char* promptText = "308sh> ";

int main(int argc, char** argv) {

	if (argc >= 2) parseArgs(argc, argv);

	while (1) {
		char in[100];
		prompt(promptText, in, sizeof(in));
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