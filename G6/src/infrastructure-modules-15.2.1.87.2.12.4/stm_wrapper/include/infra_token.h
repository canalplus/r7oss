#ifndef INFRA_TOKENIZER_H_
#define INFRA_TOKENIZER_H_

typedef enum{
	UNKNOWN = 0,
	STRING = 1,
	INTEGER = 2,
	HEX = 3
}infra_type_t;

char *get_tc_name(char *buf);

int check_for_string (char *buf, void *data_p, uint8_t size);

int check_for_integer (char *buf, void *data_p);

int get_arg(void **data_p,infra_type_t type , void *argument, uint8_t arg_size);

int get_arg_size(char *data_p);

int get_number_of_tokens(char *data_p);

//infra_error_code_t get_param_data(char *param_str, void *data_p);

#endif
