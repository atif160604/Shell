
#define _GNU_SOURCE
#include <stdbool.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>  
#define MAX_ARGS 64


#define MAX_INPUT 255

bool is_special_char(char c) {
    return (c == '<' || c == '>' || c == '|' || c == ';' || c == '(' || c == ')');
}

char** tokenize(char *input, int *token_count) {
    char **tokens = malloc(MAX_INPUT * sizeof(char*));
    int count = 0;

    char *start = input, *end = input;
    bool in_quote = false;

    while(*end) {
        if(*end == '"') {
            in_quote = !in_quote;
            if(!in_quote) {
                char temp = *end;
                *end = '\0';
                tokens[count++] = strdup(start + 1);
                *end = temp;
                start = end + 1;
            }
        } else if(!in_quote && is_special_char(*end)) {
            if(start != end) {
                char special = *end;
                *end = '\0';
                tokens[count++] = strdup(start);
                *end = special;
            }
            tokens[count++] = strndup(end, 1);
            start = end + 1;
        } else if(!in_quote && (*end == ' ' || *end == '\t' || *end == '\n')) {
            if(start != end) {
                char temp = *end;
                *end = '\0';
                tokens[count++] = strdup(start);
                *end = temp;
            }
            start = end + 1;
        }
        end++;
    }

    if(start != end) {
        tokens[count++] = strdup(start);
    }
    *token_count = count;
    return tokens;
}

void freeTokens(char** tokens, int count){

    for (int j = 0; j < count; j++) {
        free(tokens[j]);
    }
    free(tokens);
}

void handle_pipe(char **left_tokens, char **right_tokens) {
    int fd[2];
    pid_t pid1, pid2;

    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    if ((pid1 = fork()) == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execvp(left_tokens[0], left_tokens);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    if ((pid2 = fork()) == 0) {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        execvp(right_tokens[0], right_tokens);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

char* read_input() {
    char *buffer = malloc(sizeof(char) * MAX_INPUT);
    if (!buffer) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    if (!fgets(buffer, MAX_INPUT, stdin)) {
        printf("Bye bye.\n");
        exit(EXIT_SUCCESS);
    }
    return buffer;
}
void execute_command(char **tokens, int token_count, char* prev) {
    pid_t pid;
    int status;
    char **args = calloc(MAX_ARGS, sizeof(char*));
    int i, arg_count = 0;

    for (i = 0; i < token_count; i++) {
        // Built-in command: exit
        if (strcmp(tokens[i], "exit") == 0) {
            printf("Bye bye.\n");
            freeTokens(tokens, token_count);
            freeTokens(args, arg_count);
	        free(prev);
            exit(EXIT_SUCCESS);
        }
        
        // Built-in command: cd
        else if (strcmp(tokens[i], "cd") == 0) {
            if (chdir(tokens[++i]) < 0) {
                perror("cd error");
            }
            freeTokens(args, arg_count);
            return;
        }

	//Built-in command: prev
	else if(strcmp(tokens[i], "prev") == 0) {
	    printf("%s\n", prev);
	    int prev_count;
	    char** prevTokens = tokenize(prev, &prev_count);
	    execute_command(prevTokens, prev_count, "");
	    freeTokens(args, arg_count);
        freeTokens(prevTokens, prev_count);
	    return;
	}
        
        // Built-in command: source
        else if (strcmp(tokens[i], "source") == 0) {
            char *script = tokens[++i];
            FILE *fp = fopen(script, "r");
            if (fp) {
                char line[MAX_INPUT];
                while (fgets(line, sizeof(line), fp) != NULL) {
    			int script_token_count = 0;
    			char **script_tokens = tokenize(line, &script_token_count);
    			execute_command(script_tokens, script_token_count, prev);
    
    // Free each argument in the token list
    			freeTokens(script_tokens, script_token_count);
			}
                fclose(fp);
            } else {
                perror("source error");
            }
            freeTokens(args, arg_count);
            return;
        }
        
        // Built-in command: help
        else if (strcmp(tokens[i], "help") == 0) {
            printf("Built-in commands:\n");
            printf("cd [dir]       : Change directory to [dir]\n");
            printf("source [file]  : Execute commands from [file]\n");
            printf("exit           : Exit the shell\n");
            printf("help           : Show this help message\n");
            freeTokens(args, arg_count);
            return;
        }

        // Handle piping
        else if (strcmp(tokens[i], "|") == 0) {
            handle_pipe(args, &tokens[i + 1]);
            freeTokens(args, arg_count);
            return;
        }
        
        // Handle input redirection
        else if (strcmp(tokens[i], "<") == 0) {
            int fd = open(tokens[++i], O_RDONLY);
            pid_t pid = fork();
            if(pid == 0){
                dup2(fd, STDIN_FILENO);
                execvp(args[0], args);
                
            }
	    int status;
	    pid = wait(&status);
        freeTokens(args, arg_count);
            close(fd);
        }
        
        // Handle output redirection
        else if (strcmp(tokens[i], ">") == 0) {
            int fd = open(tokens[++i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            pid_t pid = fork();
            if(pid == 0)
            {
                dup2(fd, STDOUT_FILENO);
                execvp(args[0], args);
            }
	    int status;
	    pid = wait(&status);
        freeTokens(args, arg_count);
            close(fd);
        }
        
        // Handle command sequence or the end of the command list
        else if (strcmp(tokens[i], ";") == 0 || i + 1 >= token_count) {
            if (i + 1 >= token_count && strcmp(tokens[i], ";") != 0) {
                args[arg_count++] = strdup(tokens[i]);
            }
            
            if ((pid = fork()) == 0) {
                execvp(args[0], args);
                fprintf(stderr, "%s: command not found\n", args[0]);
                exit(EXIT_FAILURE);
            } else if (pid < 0) {
                perror("fork error");
            } else {
                waitpid(pid, &status, WUNTRACED);
            }
            freeTokens(args, arg_count);
        }

        // Collect arguments for a command
        else {
            args[arg_count++] = strdup(tokens[i]);
        }
    }
}



int main(int argc, char **argv) {
    char input[MAX_INPUT];
    char **tokens;
    
    char* prev = calloc(MAX_INPUT, sizeof(char));
    printf("Welcome to mini-shell.\n");

    while (1) {
        printf("shell $ ");

        // Read input
        if (!fgets(input, MAX_INPUT, stdin)) {
            printf("Bye bye.\n");
	    free(prev);
            exit(0);
        }

        // Remove trailing newline character
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') {
            input[len-1] = '\0';
        }

        // Tokenize input
        int token_count = 0;
	    tokens = tokenize(input, &token_count);


        // Execute command
        execute_command(tokens, token_count, prev);

        // Free tokens
        freeTokens(tokens, token_count);
	free(prev);
	prev = strdup(input);
    }

    return 0;
}
