CC = g++
CPPFLAGS = -std=c++11 -g

PWD = $(shell pwd)

INCLUDE = -I$(PWD)

LIB_LIST = -lutils -llog -leularcrypto -lpthread -ldl
STATIC_LIB_LIST = /usr/local/lib/libyaml-cpp.a /usr/local/lib/libhiredis.a

CORE_SRC_LIST =				\
		core/timer.cpp		\


DB_SRC_LIST = 				\
		db/redis.cpp		\
		db/redispool.cpp	\


FIBER_SRC_LIST = 			\
		fiber/fiber.cpp		\
		fiber/scheduler.cpp	\
		fiber/thread.cpp	\


NET_SRC_LIST = 				\
		net/address.cpp		\
		net/epoll.cpp		\
		net/service.cpp		\
		net/socket.cpp		\
		net/tcp_server.cpp	\
		net/udpsocket.cpp	\


PROTOCOL_SRC_LIST = 		\
		protocol/protocol.cpp

UTIL_SRC_LIST = 			\
		util/uuid.cpp		\


COMMON_SRC_LIST =			\
		app.cpp				\
		config.cpp			\
		fdmanager.cpp		\
		hook.cpp			\
		iomanager.cpp		\
		main.cpp			\
		p2p_service.cpp		\
		p2p_session.cpp		\
		session.cpp			\


CORE_OBJ_LIST 	= $(patsubst %.cpp, %.o, $(CORE_SRC_LIST))
DB_OBJ_LIST 	= $(patsubst %.cpp, %.o, $(DB_SRC_LIST))
FIBER_OBJ_LIST 	= $(patsubst %.cpp, %.o, $(FIBER_SRC_LIST))
NET_OBJ_LIST 	= $(patsubst %.cpp, %.o, $(NET_SRC_LIST))
COMMON_OBJ_LIST = $(patsubst %.cpp, %.o, $(COMMON_SRC_LIST))
PROTOCOL_OBJ_LIST = $(patsubst %.cpp, %.o, $(PROTOCOL_SRC_LIST))
UTIL_OBJ_LIST 	= $(patsubst %.cpp, %.o, $(UTIL_SRC_LIST))

main : $(CORE_OBJ_LIST) $(DB_OBJ_LIST) $(FIBER_OBJ_LIST) $(NET_OBJ_LIST) $(PROTOCOL_OBJ_LIST) $(UTIL_OBJ_LIST) $(COMMON_OBJ_LIST)
	$(CC) $^ -o $@ $(STATIC_LIB_LIST) $(LIB_LIST) 

%.o : %.cpp
	$(CC) -c $^ -o $@ $(INCLUDE) $(CPPFLAGS)


.PHONY :	\
	main clean

clean :
	-rm -f $(CORE_OBJ_LIST) $(DB_OBJ_LIST) $(FIBER_OBJ_LIST) $(NET_OBJ_LIST) $(PROTOCOL_OBJ_LIST) $(UTIL_OBJ_LIST) $(COMMON_OBJ_LIST)