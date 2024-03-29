//
// Created by Adam Mitchell on 2/19/16.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <signal.h>
#include "chop_line.h"

typedef struct {
    char **args;           //pointer to "args" null-terminated strings
    int num_args;           //size of "args" string pointer array
} program_with_args_t;

int MAX_ARGS = 32;

int valid(chopped_line_t *args);

program_with_args_t **construct_programs(chopped_line_t *parsed_line);

bool check_exit(char *line);

void handle_sigchld(int sig);


int main(int argc, char **argv) {
    size_t buffer_size = 4096;
    char *input_buffer = malloc(buffer_size);
    chopped_line_t *parsed_command;
    program_with_args_t **programs;

    // Register signal handlers
    struct sigaction action;
    action.sa_handler = &handle_sigchld;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &action, 0) == -1) {
        perror(0);
        exit(1);
    }

    int pid;
    ssize_t bytes_read = 0;
    while (true) {
        printf("simsh: ");

        bytes_read = getline(&input_buffer, &buffer_size, stdin);

        if (!strcmp(input_buffer, "\n")) {
            continue;
        }

        if (bytes_read == -1 || check_exit(input_buffer)) {
            printf("exiting \n");
            exit(1);
        }

        // parse the command arguments into an array of arrays
        parsed_command = get_chopped_line(input_buffer);

        int valid_input = valid(parsed_command);
        if (valid_input == -2) {
            printf("operator & must appear at end of command line\n");
            continue;
        }
        if (valid_input == -1) {
            printf("invalid input\n");
            continue;
        }

        // input is valid now create child process to run program
        // fork to create child process
        pid = fork();
        if (pid == -1) {
            printf("\"fork\" failed\n");
            continue;
        }

        if (pid != 0) { // parent process
            int status;
            if (valid_input != 2) { // user didn't type & must wait on child process
                waitpid(pid, &status, 0);
            }
        }
        else {// child process
            programs = construct_programs(parsed_command);

            printf("command: %s\n", programs[0]->args[0]);
            execvp(programs[0]->args[0], programs[0]->args);
            _exit(1);
        }

    }
}

program_with_args_t **construct_programs(chopped_line_t *parsed_line) {
    int i, num_processes_needed = 1;
    char *last_token_was = "";
    program_with_args_t **programs;

    for (i = 0; i < parsed_line->num_tokens; i++) {
        if (!strcmp(parsed_line->tokens[i], "|")) {
            num_processes_needed++;
        }
    }

    programs = (program_with_args_t **) malloc(num_processes_needed * sizeof(chopped_line_t));


    for (i = 0; i < num_processes_needed; i++) {
        programs[i] = (program_with_args_t *) calloc(1, sizeof(chopped_line_t));
        programs[i]->args = (char **) malloc(MAX_ARGS * MAX_ARGS * sizeof(char *));
        programs[i]->num_args = 0;
    }

    int process_index = 0;
    bool have_name = false;

    for (i = 0; i < parsed_line->num_tokens; i++) {
        char *current_token = parsed_line->tokens[i];

        if (!strcmp(last_token_was, "<")) {
            continue;
        }
        else if (!strcmp(last_token_was, ">")) {
            continue;
        }
        else if (!strcmp(last_token_was, ">>")) {
            continue;
        }
        else if (!strcmp(current_token, "|")) {
            continue;
        }
        else if (!strcmp(current_token, "<")) {
            last_token_was = strdup(current_token);
            continue;
        }
        else if (!strcmp(current_token, ">")) {
            last_token_was = strdup(current_token);
            continue;
        }
        else if (!strcmp(current_token, ">>")) {
            last_token_was = strdup(current_token);
            continue;
        }
        else if (!strcmp(current_token, "&")) {
            continue;
        }
        else if (!have_name) { // haven't found index's name
            programs[process_index]->args[0] = strdup(current_token);
            programs[process_index]->num_args = 1;

            have_name = true;
            continue;
        }
        else { // have found index's name
            int args_array_index = programs[process_index]->num_args;
            programs[process_index]->args[args_array_index] = strdup(current_token);
            programs[process_index]->num_args = args_array_index + 1;
        }
    }
    return programs;
}


int valid(chopped_line_t *args) {
    int i, num_args = args->num_tokens, num_inputs = 0, num_outputs = 0, num_ands = 0;

    for (i = 0; i < num_args; i++) {
        char *current_token = args->tokens[i];
        if (num_ands > 0) {
            return -2;
        }
        if (!strcmp(current_token, "&")) {
            num_ands++;
        }
        if (!strcmp(current_token, "<")) {
            num_inputs++;
        }
        if (!strcmp(current_token, ">>") || !strcmp(current_token, ">")) {
            num_outputs++;
        }
    }

    if (num_inputs > 1 || num_outputs > 1) {
        return -1;
    }
    if (num_ands == 1) {// there is a & in the command line so don't wait for child.
        return 2;
    }
    // no & in command line must wait on child.
    return 1;
}

bool check_exit(char *line) {
    chopped_line_t *parsed = get_chopped_line(line);
    if (!strcmp(parsed->tokens[0], "exit")) {
        return true;
    }
    return false;
}


void handle_sigchld(int sig) {
    int saved_errno = errno;
    while (waitpid((pid_t) (-1), 0, WNOHANG) > 0) { }
    errno = saved_errno;
}
