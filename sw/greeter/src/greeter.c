#include "greeter.h"
#include "hello_output.h"

const char *greeter_get_message(void) { return "Hello, World!"; }

void greeter_run(void) { hello_output_write_line(greeter_get_message()); }
