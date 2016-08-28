include ../../build_config.mk

OBJS = lua_proc_kv.o lua_proc_m.o lua.o
UTIL_OBJS = ../util/log.o ../util/config.o ../util/bytes.o
EXES = test

all: ${OBJS}
	ar -cru ./liblua.a ${OBJS}

lua_proc_kv.o: lua.h lua_proc_kv.cpp
	${CXX} ${CFLAGS} -c lua_proc_kv.cpp

lua_proc_m.o: lua.h lua_proc_m.cpp
	${CXX} ${CFLAGS} -c lua_proc_m.cpp

lua.o: lua.h lua.cpp
	${CXX} ${CFLAGS} -c lua.cpp

test:
	${CXX} -o test.out test.cpp ${CFLAGS} ${OBJS} ${UTIL_OBJS} ${CLIBS}
	${CXX} -o test2.out test2.cpp ${CFLAGS} ${OBJS} ${UTIL_OBJS} ${CLIBS}

clean:
	rm -f ${EXES} *.a *.o *.exe
