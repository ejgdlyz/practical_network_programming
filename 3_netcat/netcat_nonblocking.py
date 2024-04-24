#!/usr/bin/python

import errno
import fcntl
import os
import select
import socket
import sys

def setNonBlocking(fd):
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)


def nonBlcokingWrite(fd, data):
    try:
        nw = os.write(fd, data)
        return nw
    except OSError as e:
        if e.errno == errno.EWOULDBLOCK:
            return -1

def relay(sock):
    socketEvents = select.POLLIN
    poll = select.poll()
    poll.register(sock, socketEvents)
    poll.register(sys.stdin, select.POLLIN)  # stdin only cares about input event

    setNonBlocking(sock)
    # setNonBlocking(sys.stdin)
    # setNonBlocking(sys.stdout)

    done = False
    stdoutOutputBuffer = ""
    socketoutputBuffer = ""
    while not done:
        events = poll.poll(10000)  # 10 seconds
        for fileno, event in events:
            if event & select.POLLIN:  # input event
                if fileno == sock.fileno():  # socketfd
                    data = sock.recv(8192)
                    if data:
                        nw = sys.stdout.write(data.decode('utf-8'))  # stdout does not support non-blocking write
                    else:
                        done = True
                else:   # stdin
                    assert fileno == sys.stdin.fileno()
                    data = os.read(fileno, 8192)
                    if data:
                        assert len(socketoutputBuffer) == 0
                        nw = nonBlcokingWrite(sock.fileno(), data)    # non-blockingly write data to socketfd
                        if nw < len(data):      # not finish writting, i.e. short write
                            if nw < 0:
                                nw = 0
                            socketoutputBuffer = data[nw:]  # remaining data
                            socketEvents |= select.POLLOUT  # add pollout event
                            poll.register(sock, socketEvents)
                            poll.unregister(sys.stdin)      # only for this netcat example
                    else:   # stdin over
                        sock.shutdown(socket.SHUT_WR)
                        poll.unregister(sys.stdin)
            if event & select.POLLOUT:              # pollout event happens
                if fileno == sock.fileno():
                    assert len(socketoutputBuffer) > 0
                    nw = nonBlcokingWrite(sock.fileno, socketoutputBuffer)
                    if nw < len(socketoutputBuffer):    # not finishing writting
                        assert nw > 0
                        socketoutputBuffer = socketoutputBuffer[nw:]
                    else:                               # finishing writting
                        socketoutputBuffer = ""
                        socketEvents &= ~select.POLLOUT     # must remove pollout event
                        poll.register(sock, socketEvents)   
                        poll.register(sys.stdin, select.POLLIN) # only for this netcat example


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
