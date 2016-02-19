#$(CPLUSPLUS) -static  -o $@ $(OBJS) $(LIBPATH) $(LIB)
CPLUSPLUS = g++
CFLAGS = -g
INCLUDE_ROOT = ./
LIBPATH = -L/usr/local/lib64
STRIP=strip
SRC  = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp,%.o,$(SRC))

LIB = -Wl,--start-group -lpthread -lm \
	    -levent -lrt -ldl \
		-Wl,--end-group

APP = test

all:$(APP)

%.o:%.cpp 
	$(CPLUSPLUS) $(CFLAGS)  -c $< -o $@ $(INCLUDE) 

$(APP):$(OBJS)
	$(CPLUSPLUS) -o $@ $(OBJS) $(LIBPATH) $(LIB)
	$(STRIP) $(APP)
	rm -rf $(OBJS)

clean:
	rm -rf $(APP)
	rm -rf $(OBJS)
