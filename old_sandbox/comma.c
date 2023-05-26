#include <stdio.h>

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <libgen.h>

/******************************************************************************/
  /** @brief   returns a string with the value with commas inserted
   *
  */
  char * sprintf_commas( int64_t value )
  {
      static char buf[20];

      sprintf(buf, "%" PRIi64, value);
      int len = strlen(buf);

      printf("len %d buf %s\n", len, buf);

      for (int i = len - 3; i > 0; i -= 3) 
      {
          printf("before: i %d, len %d, buf %s\n", i, len, buf);
          memmove(&buf[i+1], &buf[i], len - i+1);
          buf[i] = ',';
          //buf[len+2] = '\0';
          len = strlen(buf);
          printf("after: i %d, len %d, buf %s\n", i, len, buf);
      }

      return buf;
  }

char * printfcomma3 (uint64_t n) {
    static char buf[50];
    char * new_buf;

    if (n < 1000) {
        sprintf (buf, "%ld", n);
        printf("buf: %s \n", buf);
        return buf;
    }
    new_buf = printfcomma3 (n/1000);
    printf("new_buf %s \n", new_buf);
    strcat(buf, new_buf);
    printf("buf: %s \n", buf);

    return buf;
}

void printfcomma2 (uint64_t n) {
    if (n < 1000) {
        printf ("%ld", n);
        return;
    }
    printfcomma2 (n/1000);
    printf (",%03lu", n%1000);
}

void printfcomma (uint64_t n) {
    if (n < 0) {
        printf ("-");
        n = -n;
    }
    printfcomma2 (n);
}

int main(void)
{
    uint64_t a = 1;

#ifdef poo
    for (int i = 0; i < 19 ; i++)
    {
        printfcomma(a);
        printf(" %ld \n", a);
        a = a * 10;
    }
    printf("\n\n\n");
#endif

    char * str_ptr;
    a = -1;
    for (int i = 0; i < 19 ; i++)
    {
        str_ptr = sprintf_commas(a);
        printf("done: %s  %ld, ptr %p \n\n\n", str_ptr, a, str_ptr);
        a = a * 10;
    }

#ifdef poo
    a = -1;
    for (int i = 0; i < 19 ; i++)
    {
        printf("done: %s  %ld \n\n\n", sprintf_commas(a), a);
        a = a * 11;
    }
#endif


}
