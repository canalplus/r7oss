#ifndef CA_TOKENIZER_H_
#define CA_TOKENIZER_H_

typedef enum{
	UNKNOWN = 0,
	STRING = 1,
	INTEGER = 2,
	HEX = 3
}ca_type_t;

char *ca_get_tc_name(char *buf);

int ca_check_for_string (char *buf, void *data_p, uint8_t size);

int ca_check_for_integer (char *buf, void *data_p);

int ca_get_arg(void **data_p,ca_type_t type , void *argument, uint8_t arg_size);

int ca_ca_get_arg_size(char *data_p);

int ca_get_number_of_tokens(char *data_p);


#endif
