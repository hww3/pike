void f_aap_log_as_array(INT32 args);
void f_aap_log_exists(INT32 args);
void f_aap_log_size(INT32 args);
void f_aap_log_as_commonlog_to_file(INT32 args);
void aap_log_append(int sent, struct args *arg, int reply);


extern struct log *aap_first_log;
extern struct program *aap_log_object_program;
