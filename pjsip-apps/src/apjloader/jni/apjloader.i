%module apjloader

%{
int main(int argc, char **argv);

#ifdef __cplusplus
extern "C" {
#endif
    int init_stdio_pipe();
    void destroy_stdio_pipe();
    int read_from_stdout(char *ch);
    int write_to_stdin(const char *st);
#ifdef __cplusplus
}
#endif

%}

%include "various.i"

%apply char **STRING_ARRAY { char **argv };
int main(int argc, char **argv);

%include "typemaps.i"
%apply signed char *INOUT { char *ch };

int init_stdio_pipe();
void destroy_stdio_pipe();
int read_from_stdout(char *ch);
int write_to_stdin(const char *st);
