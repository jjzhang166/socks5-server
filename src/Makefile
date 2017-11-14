include ../Makefile.inc

GEN_EXE =

LINUX = evsocks

EXE = ${GEN_EXE} ${LINUX}

all : ${EXE}

allgen : ${GEN_EXE}

LDLIBS = ${IMPL_LDLIBS} -levent

clean : ${RM} ${EXE} *.o

showall :
	@echo ${EXE}
