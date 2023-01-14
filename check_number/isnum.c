// C++ program to check if input number // is a valid number
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
 
uint64_t valid_number(char * str)
{
    bool isgig = false;
    bool ismeg = false;
    int i = 0, j = strlen(str) -1 ;
 
    // Handling whitespaces
    while (i < strlen(str) && str[i] == ' ')
        i++;
    while (j >= 0 && str[j] == ' ')
        j--;
 
    if (i > j)
        return 0;
 
    // if string is of length 1 and the only
    // character is not a digit
    if (i == j && !(str[i] >= '0' && str[i] <= '9'))
        return 0;
 
    // If the 1st char is not '+', '-', '.' or digit
    if (str[i] != '.' && str[i] != '+'
        && str[i] != '-' && !(str[i] >= '0' && str[i] <= '9'))
        return 0;
 
    // To check if a '.' or 'e' is found in given
    // string. We use this flag to make sure that
    // either of them appear only once.
    bool flagDotOrE = false;
 
    for (i; i <= j; i++) {
        printf("i %d, j %d, str[i] %c\n", i, j, str[i]);

        // If any of the char does not belong to
        // {digit, +, -, ., e, g, G, m, M}
        if (str[i] != 'e' && str[i] != '.'
            && str[i] != '+' && str[i] != '-'
            && str[i] != 'g'
            && str[i] != 'G'
            && str[i] != 'm'
            && str[i] != 'M'
            && !(str[i] >= '0' && str[i] <= '9' ))
        {
            printf("leaving cause wrong alpha\n");
            return 0;
        }
 
        if (str[i] == '.') {
            // checks if the char 'e' has already
            // occurred before '.' If yes, return 0.
            if (flagDotOrE == true)
                return 0;
            else
            {
                flagDotOrE = true;
            }

 
            // If '.' is the last character.
            if (i + 1 > strlen(str))
                return 0;
 
            // if '.' is not followed by a digit.
            if (!(str[i + 1] >= '0' && str[i + 1] <= '9'))
                return 0;
        }
 
        else if (str[i] == 'e') {
            // set flagDotOrE = 1 when e is encountered.
            flagDotOrE = true;
 
            // if there is no digit before 'e'.
            if (!(str[i - 1] >= '0' && str[i - 1] <= '9'))
                return 0;
 
            // If 'e' is the last Character
            if (i + 1 > j)
                return 0;
 
            // if e is not followed either by
            // '+', '-' or a digit
            if (str[i + 1] != '+' && str[i + 1] != '-'
                && (str[i + 1] >= '0' && str[i] <= '9'))
                return 0;
        }
        else if ((str[i] == 'g' || str[i] == 'G') && (i == j))
        {
            printf("is gig\n");
            isgig = true;
        }
        else if ((str[i] == 'm' || str[i] == 'M') && (i == j))
        {
            printf("is meg\n");
            ismeg = true;
        }

    }
 
    /* If the string skips all above cases, then
    it is numeric*/

    uint64_t final_int = 0;
    double final_double = 0;
    uint64_t final_val = 0;

    /* if Dot or e then it is a floating point number */
    if (flagDotOrE)
    {
        final_double = strtod(str, NULL);
        if (isgig == true)
        {
            
            final_double *= 1000000000;
        }
        else if (ismeg == true)
        {
            final_double *= 1000000;
        }

        final_val = (uint64_t) final_double;
    }
    else
    {
        final_val = strtold(str, NULL);
        if (isgig == true)
        {
            
            final_val *= 1000000000;
        }
        else if (ismeg == true)
        {
            final_val *= 1000000;
        }
    }



    return final_val;
}
 
// Driver code
int main()
{
    
    char str[100] = "*";

    while (strlen(str) > 0)
    {
        printf("Enter input: \n");
        scanf("%s", str);

        uint64_t parsed_num = valid_number(str);

        if (parsed_num != 0)
        {
            printf("Number: %ld\n", parsed_num);
            printf("true\n");
        }
        else
        {
            printf("false\n");
        }
    }
    return 0;
}
 
