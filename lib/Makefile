include ../Makefile.inc

# The library build is "brute force" -- we don't bother with 
# dependency checking.

allgen : ${APP_LIB}

${APP_LIB} : internal.c
	${CC} -c -g ${CFLAGS} *.c
	${RM} ${APP_LIB}
	${AR} rs ${APP_LIB} *.o

clean :
	${RM} *.o ${APP_LIB}
