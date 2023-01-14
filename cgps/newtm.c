#include <time.h>
#include <stdio.h>



struct tm update_tm(struct tm utc_tm, int secs)
  {
      time_t utc_secs = mktime(&utc_tm);
      struct tm new_tm = *localtime( &utc_secs);
  
      new_tm.tm_sec += secs;
  
      mktime( &new_tm);
  
      printf("num secs %d\n", secs);
      printf( "%s\n", asctime( &utc_tm));
      printf( "%s\n", asctime( &new_tm));
  
      return new_tm;
  }

int main()
{
    time_t now = time(NULL);

    struct tm now_tm = *localtime(&now);

    struct tm new_tm  = update_tm(now_tm, 50);

}


