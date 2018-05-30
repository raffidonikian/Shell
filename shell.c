#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);


/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_cd, "cd", "change directory"},
  {cmd_pwd, "pwd", "print working directory aka current directory"},
  {cmd_wait, "wait", "chills out for a minute"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

int cmd_wait(unused struct tokens *tokens) {
  int status;
  while (wait(&status) != -1) {

  }
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

int cmd_pwd(unused struct tokens *tokens) {
  char wd[1000];
  getcwd(wd, 1000);
  printf("%s\n", wd);
  return 1;
}

int cmd_cd(unused struct tokens *tokens) {
  int x = chdir(tokens_get_token(tokens, 1));
  if (x == 0) {
    return 1;
  }
  else {
    printf("Couldn't do it, sorry =/");
    return 0;
  }
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    }
    else {
      /* REPLACE this to run commands as programs. */
      int argcp = tokens_get_length(tokens);
      char **argvp = malloc((argcp + 1) * sizeof(char*));
      int writeOut = 0;
      int readIn = 0;
      int background = 0;
      int x;
      char *filename = (char*) malloc(1000);
      for(int i = 0; i < argcp; i++) {
          char* temp = tokens_get_token(tokens, i);
          if (strcmp(temp, "<") == 0) {
            readIn = 1;
            filename = tokens_get_token(tokens, i + 1);
          }
          if (strcmp(temp, ">") == 0) {
            writeOut = 1;
            filename = tokens_get_token(tokens, i + 1);
          }
          if (strcmp(temp, "&") == 0) {
            background = 1;
          }
          if (writeOut + readIn < 1) {
            if (background == 0) {
            argvp[i] = tokens_get_token(tokens, i);
            }
          }

      }
      pid_t pid = fork();
      if (pid == -1) {
        printf("Fork Failed");
      }
      else if (pid == 0){
        //running programs here
        signal(SIGINT, SIG_DFL);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);



        printf("%d hELOOOOOO", background);
        if (!background) {
          tcsetpgrp(0, getpgid(0));
			  }
        int fd = 0;
        if (writeOut > 0) {
          fd = open(filename, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
          dup2(fd, 1);
        }
        if (readIn > 0) {
          fd = open(filename, O_RDONLY, S_IRUSR|S_IWUSR);
          dup2(fd, 0);
        }

        if(execv(tokens_get_token(tokens, 0), argvp) == -1) {
          struct tokens *possible_paths = tokenize(getenv("PATH"));
          char *fin_path = (char*) malloc(1000);
          for (int i = 0; i < tokens_get_length(possible_paths); i++) {
            char *p = tokens_get_token(possible_paths, i);
            strcpy(fin_path, p);
            strcat(fin_path,"/");
            strcat(fin_path, tokens_get_token(tokens, 0));
            execv(fin_path, argvp);
          }
          printf("couldn't run program");
          exit(0);
        }
      }
      else {
        signal(SIGINT, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        setpgid(pid, pid);
        if (!background) {
          waitpid(pid, &x, 0);
          tcsetpgrp(0, shell_pgid);
			  }
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
