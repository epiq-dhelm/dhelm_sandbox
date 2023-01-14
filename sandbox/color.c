#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <inttypes.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <strings.h>

  char *level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

  char *level_colors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};


int main(void)
{
 printf(
    "%s<s%-5s>\x1b[0m \n",
    level_colors[0], level_strings[0]);

    char word[90];
    sprintf(word, "<%s>", level_strings[2]);
    int width = 9;

    // No truncation uses %-*s
    printf( "%-*s %s\n", width, word, "Ok." );
    // Output is "Starting initialization...     Ok."

    sprintf(word, "<%s>", level_strings[1]);
    // No truncation uses %-*s
    printf( "%-*s %s\n", width, word, "Ok." );
    // Output is "Starting initialization...     Ok."

    // Truncated to the specified width uses %-.*s
    printf( "%-.*s %s\n", width, "Starting initialization...", "Ok." );
    // Output is "Starting initialization... Ok."
}
