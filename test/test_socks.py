#!/usr/bin/env python

# Watch out - this is a work in progress.
# Simple test for sock servers both local and remote mode
# call Curl to see how it goes.

from __future__ import print_function
import os
import sys
import shlex
import subprocess
import multiprocessing as mp
import time
import signal


ENVFLAGS = {'DEBUG': 'yes'}
REMOTE_ADDR = "0.0.0.0"
REMOTE_PORT = 4001
LOCAL_ADDR = "127.0.0.1"
LOCAL_PORT = 4000
PASSWORD = "this-is-a-password"

addrs = [
    "http://google.com",
    # "https://google.com",
    # "https://ipv6.google.com"
]


# Curl return code:
# https://curl.haxx.se/libcurl/c/libcurl-errors.html
ERR_CODE = dict(
    CURL_SSL_CONNECT_ERR=35,
    CURL_GOT_NOTHING=52,
    CURL_SEND_ERR=55,
)


def create_srv_proc(cmd):
    proc = subprocess.Popen(
        shlex.split(cmd), stdin=subprocess.PIPE, stderr=subprocess.PIPE,
        env=ENVFLAGS, cwd="..")
    out, err = proc.communicate()
    print(err)
    return proc


def call_cmd(cmd):
    proc = subprocess.Popen(
         shlex.split(cmd), stdin=subprocess.PIPE, stderr=subprocess.PIPE,
         env=ENVFLAGS)
    out, err = proc.communicate()
    print(err)
    if proc.returncode in ERR_CODE.values():
        i = 0
        while True:
            try:
                code = ERR_CODE.values()[i]
                i += 1
            except IndexError:
                break
        raise Exception(ERR_CODE.keys()[i-2])


def start_servers():
    remote = """esocks --server_addr {raddr} --server_port {rport}
          --local_addr {laddr} --local_port {lport} --password {password}
    """.format(raddr=REMOTE_ADDR, rport=REMOTE_PORT,
               laddr=LOCAL_ADDR, lport=LOCAL_PORT,
               password=PASSWORD)

    local = """esocks --local_addr {laddr} --local_port {lport}
    --server_addr {raddr} --server_port {rport} --local
    --password {password} --local
    """.format(laddr=LOCAL_ADDR, lport=LOCAL_PORT,
               raddr=REMOTE_ADDR, rport=REMOTE_PORT,
               password=PASSWORD)

    proc1 = mp.Process(target=create_srv_proc, args=(remote,))
    setattr(proc1, 'daemon', True) # Run servers background
    proc1.start()
    
    proc2 = mp.Process(target=create_srv_proc, args=(local,))
    setattr(proc2, 'daemon', True) # Run servers background
    proc2.start()

    return proc1, proc2


def test_response(target):
    curl = """curl --socks5 {addr}:{port} {tg} -L -vv""".format(
        addr=LOCAL_ADDR, port=LOCAL_PORT, tg=target)
    proc3 = mp.Process(target=call_cmd, args=(curl,))
    proc3.start()    
    assert proc3.is_alive()


if __name__ == "__main__":

    p1, p2 = start_servers()
    [test_response(x) for x in addrs]

    # Wait for process completion
    time.sleep(.4)

    p1.terminate()
    p2.terminate()

    print("remote",p1.pid)
    print("local",p2.pid)
    
    assert not p1.is_alive()
    assert not p2.is_alive()
    
    print("stopped!")
