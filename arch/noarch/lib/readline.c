#include "readline.h"

int
readline(int(*getc)(void *priv), void(*putc)(int ch,void *priv), void *priv, char *buf, int size) {
    int i = 0, c;
    while (1) {
        c = getc(priv);
        if (c < 0) {
            return -1;
        }
        else if (c >= ' ' && i < size - 1) {
            putc(c, priv);
            buf[i ++] = c;
        }
        else if (c == '\b' && i > 0) {
            putc(c, priv);
            i --;
        }
        else if (c == '\n' || c == '\r') {
            putc(c, priv);
            buf[i] = '\0';
            return i;
        }
    }
    return -1;
}
