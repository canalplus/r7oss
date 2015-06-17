/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cectest/cec_token.c
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/*
   @file     cec_token.c
   @brief

 */
#include <linux/string.h>
#include <linux/stddef.h>
#include <linux/kernel.h>


#include "cec_token.h"
#include "cec_test_interface.h"

#define SPACE 32

int get_arg(void **, cec_type_t , void *, uint8_t );
int get_arg_size(char *);
int get_number_of_tokens(char *);
int check_for_string(char *, void *, uint8_t );
int check_for_integer(char *, void * );
int get_integer_from_ascii(const char *);
int get_param_data(char *, void *);

int get_number_of_tokens(char *data_p)
{
    int len = 0, size;
    const char delimiters[] =" ";
    char *token_p, *data_copy_p;
    char *data_start_p;

    if(data_p == NULL) return len;

    size = strlen(data_p);
    data_copy_p = cec_allocate(size+1); //+1 for terminating null char
    data_start_p = data_copy_p;
    strcpy(data_copy_p,data_p);

    while(1){
        token_p = strsep(&data_copy_p, delimiters);
        while((token_p != NULL) &&  (strcmp(token_p,"")== 0) ){
            token_p = strsep(&data_copy_p,delimiters);
        }

        if(token_p == NULL) {
            cec_free(data_start_p);
            return len;
        }
        len++;
    }
    cec_free(data_start_p);
    return len;
}

int get_arg_size(char *data_p)
{
    int size;
    size = strlen(data_p)*sizeof(char);

    return size;
}

int
get_arg(void **data_pp, cec_type_t type, void *argument_p, uint8_t arg_size)
{
    int error = 0;
    int size;
    const char delimiters[] = " ";
    char *data_copy_p, *token_p;
    void *received_data_p;

    cec_test_debug(" %s>>>> %d>>> ",__func__, __LINE__);
    received_data_p = *data_pp;
    data_copy_p = (char *)received_data_p;
    size = strlen(data_copy_p) * sizeof(char);
    token_p = strsep(&data_copy_p, delimiters);

    while((token_p != NULL) &&  (strcmp(token_p,"")== 0) ){
        token_p = strsep(&data_copy_p,delimiters);
    }

    cec_test_debug("\tSeparated Token : %s", token_p);

    if(token_p == NULL){
        error = -EINVAL;
        cec_test_debug("\n\n\t>>>>token_p = (NULL) Error : %d<<<<<\n\n",
                error);
        return -EINVAL;
    }

    *data_pp = (void *)data_copy_p;

    switch(type){
        case STRING :

            cec_test_debug("\tChecking for String");
            error = check_for_string(token_p, argument_p, size);
            cec_test_debug( "\tcase STRING: Error (%d) ", error);
            if (error == 0)
                return size;
            else
                return error;


        case INTEGER :

            cec_test_debug("\tChecking %s  for Integer Type",
                    (char*)token_p);
            error = check_for_integer(token_p, argument_p);
            cec_test_debug( "\tcase INTEGER: Error (%d) ", error);
            return error;

        case HEX :

            cec_test_debug("\tChecking %s  for Integer Type",
                    (char*)token_p);
            error = check_for_integer(token_p, argument_p);
            cec_test_debug( "\tcase HEX: Error (%d) ", error);
            return error;

        case UNKNOWN :
            error = -EINVAL;
            cec_test_debug( "\tcase UNKNOWN: Error (%d) ", error);
            return -EINVAL;
    }

    cec_test_debug(" %s<<<< %d <<<< ",__func__, __LINE__);
    return error;
}

int check_for_string(char *data, void *argument, uint8_t size)
{
    int len,i;
    int error = 0;

    cec_test_debug(" %s>>>> %d>>> ",__func__, __LINE__);
    len = strlen(data);
    cec_test_debug( " Data '%s' Recieved",data);
    cec_test_debug("\n Length of String is : \t %d ", len);

    for(i=0; i<len; i++){
        cec_test_debug("Checking %c \t", data[i]);

        switch(data[i]){
            case 'a' ... 'z' :
            case 'A' ... 'Z' :
            case '_' :
                error = 0;
                break;

            default:
                error = -EINVAL;
                return error;
                break;
        }
    }

    cec_test_debug("Checked data %s", data);
    strncpy((char *)argument,data,size);
    ((char *)argument)[size]='\0';
    cec_test_debug(" %s<<<< %d <<<< ",__func__, __LINE__);

    return error;
}

int check_for_integer(char *data, void *argument)
{

    int error,i,len;
    int  integer_value;
    long integer_value_long;

    /*Check for integer type */

    len = strlen(data);
    data[len] = '\0';
    cec_test_debug(" %s>>> %d>>>  ",__func__, __LINE__);
    cec_test_debug( "\tData '%s' Recieved",data);

    if((data[0] == '0') && ( data[1] == 'x' || data[1] == 'X') ){
        for(i=2; i<len; i++){

            switch(data[i]){
                case '0' ... '9' :
                case 'a' ... 'f' :
                case 'A' ... 'F' :
                case '\n' :
                    error = 0;
                    break;

                default :
                    error = -EINVAL;
                    return error;
                    break;
            }
        }

        error = kstrtol(data, 0, &integer_value_long);

        integer_value = (int)integer_value_long;

    }
    else{
        cec_test_debug("\tChecking for hex");
        for(i=0; i<len; i++){
            switch(data[i]){
                case '0' ... '9':
                case  '\n'  :
                    error = 0;
                    break;
                default :
                    error = -EINVAL;
                    return error;
            }
        }

        integer_value = get_integer_from_ascii(data);
    }

    cec_test_debug( "\tConverted String ' %s ' to integer ' %d '" , data,
            integer_value);

    *(int *)argument = integer_value;
    cec_test_debug(" %s<<< %d <<<  ",__func__, __LINE__);

    return error;
}

int get_integer_from_ascii(const char *s)
{
    static const char digits[] = "0123456789";  /* legal digits in order */
    unsigned val=0;         /* value we're accumulating */
    int neg=0;              /* set to true if we see a minus sign */

    /* skip whitespace */
    while (*s==' ' || *s=='\t') {
        s++;
    }

    /* check for sign */
    if (*s=='-') {
        neg=1;
        s++;
    }
    else if (*s=='+') {
        s++;
    }

    /* process each digit */
    while (*s) {
        const char *where;
        unsigned digit;

        /* look for the digit in the list of digits */
        where = strchr(digits, *s);
        if (where==NULL) {
            /* not found; not a digit, so stop */
            break;
        }

        /* get the index into the digit list, which is the value */
        digit = (where - digits);

        /* could (should?) check for overflow here */

        /* shift the number over and add in the new digit */
        val = val*10 + digit;

        /* look at the next character */
        s++;
    }

    /* handle negative numbers */
    if (neg) {
        return -val;
    }

    /* done */
    return val;
}
