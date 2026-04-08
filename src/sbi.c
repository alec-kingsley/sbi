#include "interpreter.h"
#include "reporter.h"
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
    Interpreter *interpreter;
    if (argc != 2) {
        report_error("usage: sbi <filename>");
        goto main_fail;
    }
    srand(time(NULL));
    interpreter = interpreter_create(argv[1]);
    if (interpreter == NULL) goto main_fail;
    interpreter_run(interpreter);

    interpreter_destroy(interpreter);
    return 0;
main_fail:
    return 1;
}
