# server with libevent
一、

workermanager---|-----worker------|handlethread
            ----|                 |handlethread
            ----|                 |handlethread
            ----|                 |handlethread
            
            ----|-----worker------|handlethread
            ----|                 |handlethread
            ----|                 |handlethread
            ----|                 |handlethread
            
            ----|-----worker------|handlethread
            ----|                 |handlethread
            ----|                 |handlethread
            ----|                 |handlethread
                      |
                      |
                      |
二、

client--->workermanager accpet--->putsocket(sockpair)--->worker--->session--->addEvent 
