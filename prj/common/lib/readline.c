#include "low_io.h"

/* *
 * readline - get a line from stdin
 * @prompt:   the string to be written to stdout
 *
 * The readline() function will write the input string @prompt to
 * stdout first. If the @prompt is NULL or the empty string, no prompt
 * is issued.
 *
 * This function will keep on reading characters and saving them to
 * buffer 'buf' until '\n' or '\r' is encountered.
 *
 * Note that, if the length of string that will be read is longer than
 * buffer size, the end of string will be discarded.
 *
 * The readline() function returns the length of text read. If some
 * errors are happened, -1 is returned.
 *
 */
int
readline(const char *prompt, char *buf, int size) {
    if (prompt != NULL) {
        cprintf("%s", prompt);
    }
    int i = 0, c;
    while (1) {
        c = cgetchar();
        if (c < 0) {
            return -1;
        }
        else if (c >= ' ' && i < size - 1) {
            cputchar(c);
            buf[i ++] = c;
        }
        else if (c == '\b' && i > 0) {
            cputchar(c);
            i --;
        }
        else if (c == '\n' || c == '\r') {
            cputchar(c);
            buf[i] = '\0';
            return i;
        }
    }
	return -1;
}
