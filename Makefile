#Makefile for ls

PROG=	ls
SRCS=	ls.c print.c util.c

CC?=	gcc
CFLAGS+= -Wall -Wextra -Werror -std=c99 -pedantic

NOMAN=	yes

LDADD=	

.include <bsd.prog.mk>