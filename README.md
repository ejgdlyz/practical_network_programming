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