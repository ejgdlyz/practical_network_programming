#!/usr/bin/python

# 使用 IO 复用实现 netcat

import os
import select
import socket
import sys

def relay(sock):
    poll = select.poll()
    poll.register(sock, select.POLLIN)      # 为 sock 注册读事件
    poll.register(sys.stdin, select.POLLIN) # 为 stdin 注册读事件

    done = False
    while not done:
        events = poll.poll(1000)  # 10 seconds, 等待事件
        for fileno, event in events:
            if event & select.POLLIN:
                if fileno == sock.fileno():  # socket 可读
                    data = sock.recv(8192)
                    if data:
                        sys.stdout.write(data.decode('utf-8'))  # call c 中的 fwrite()
                    else:                       # 读到 0 就退出，不需要像 netcat.cc 那样还要通知另外的线程
                        done = True             # 数据读完才能关闭链接，否则会使 TCP 发送 RST 分节，导致数据接收不完整
                else:
                    assert fileno == sys.stdin.fileno()
                    data = os.read(fileno, 8192)
                    if data:
                        sock.sendall(data)
                    else:
                        sock.shutdown(socket.SHUT_WR)
                        poll.unregister(sys.stdin)
                        # 不直接 done = True，是需要等数据发送完，否则数据发送不完整（as nc_sender.cc)


def main(argv):
    if len(argv) < 3:
        binary = argv[0]
        print("Usage:\n  %s -l port\n  %s host port" % (argv[0], argv[0]))
        return
    port = int(argv[2])
    if argv[1] == "-l":
        # server
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind('', port)
        server_socket.listen(5)
        (client_socket, client_address) = server_socket.accept()
        server_socket.close()
        relay(client_socket)
    else:
        # client
        sock = socket.create_connection((argv[1], port))
        relay(sock)


if __name__ == "__main__":
    main(sys.argv)