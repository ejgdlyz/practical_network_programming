# Practical Network Prgramming
The C++ implementation of "Practical Network Prgramming" by chenshuo.

## 1 TTCP 
1. The ttcp blocking implementation.
    ```shell
    # shell 1
    $ while true; do ./tests/ttcp_blocking -r; done

    #shell 2
    $ ./tests/ttcp_blocking -t localhost -l 10240
    $ ./tests/ttcp_blocking -t localhost -l 1024000
    ```
2. The simple echo server/client also may be blocked.
    ```shell
    # shell 1
    $ ./tests/echo_server 127.0.0.1 3100

    # shell 2
    $ ./tests/echo_client 127.0.0.1 3100 102400
    $ ./tests/echo_client 127.0.0.1 3100 2048000
    # blocking test
    $ ./tests/echo_client 127.0.0.1 3100 20480000  

    ```

## 2 Roundtrip 
1. how to use roundtrip_udp.cc to measure clock error. 
   ``` shell
   # shell 1 as server
   $ ./tests/roundtrip_udp -s 3100

   # shell 2
   $ ./tests/roundtrip_udp 127.0.0.1 3100
   ```

## 3 Netcat
1. 如何正确使用 TCP
   
    主要体现在如何正确关闭 TCP 连接。
   - 在注释了 nc_sender 的 safe close conection 情况下，如果 nc_receiver 在 nc_sender sleep(10) 期间发送了数据，那么将导致其最终无法收到完成的数据。
   - 使用 nc_sender 的 safe close conection，无论 nc_receiver 在 nc_sender sleep(10) 期间发不发送数据都能完全接收数据。
   ```shell
    # shell 1
    $ ./nc_sender ttcp_blocking 1234 
    # shell 2
    $ ./nc_receiver 127.0.0.1 1234
   ```
2. 使用 TCP 注意事项
   - `SIGPIPE`：
    在所有并发 TCP 服务器中忽略 SIGPIPE 信号。
   - Nagle's Algorithm：
    Nale's Algorithm 可以减少网络延迟，但可能会增加应用程序延迟。
    ```shell
    # shell 1
    $ ./nodelay_server 

    # shell 2
    $ ./nodelay_client localhost 1000
    $ ./nodelay_client -b localhost 1000
    $ ./nodelay_client -D localhost 1000

    # shell 3
    $ sudo tcpdump -i lo -n tcp port 3210
    ```
    TCP Client/Server 三部曲：
    - 打开 `SO_REUSEADDR`
    - 忽略 `SIGPIPE`
    - 打开 `TCP_NODELAY`