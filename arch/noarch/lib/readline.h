#ifndef __READLINE_H__
#define __READLINE_H__

int readline(int(*getc)(void *priv), void(*putc)(int ch,void *priv), void *priv, char *buf, int size);

#endif
