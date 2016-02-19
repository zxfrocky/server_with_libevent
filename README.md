# server with libevent
一 线程运行图如下


workermanager---|-----worker------|masterthread(负责libevent事件循环 读，写，通知，getsocket)
            ----|                 |handlethread(负责处理接收到的数据)
            ----|                 |handlethread(负责处理接收到的数据)
            ----|                 |handlethread(负责处理接收到的数据)
            
            ----|-----worker------|handlethread(负责libevent事件循环 读，写，通知，getsocket)
            ----|                 |handlethread(负责处理接收到的数据)
            ----|                 |handlethread(负责处理接收到的数据)
            ----|                 |handlethread(负责处理接收到的数据)
            
            ----|-----worker------|handlethread(负责libevent事件循环 读，写，通知，getsocket)
            ----|                 |handlethread(负责处理接收到的数据)
            ----|                 |handlethread(负责处理接收到的数据)
            ----|                 |handlethread(负责处理接收到的数据)
二、
client--->workermanager accpet--->putsocket(sockpair)--->worker--->session--->addEvent 
