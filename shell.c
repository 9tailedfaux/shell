#include <stdio.h>

void prompt();

int main(int argc, char** argv) {
	prompt(argv[1]);
}

void prompt(char* prompt){
	prints(prompt);
}