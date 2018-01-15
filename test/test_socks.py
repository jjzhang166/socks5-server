#!/usr/bin/env python

# Watch out - this is a work in progress.
# Simultiprocessingle test for sock servers both local and remote mode
# call Curl to see how it goes.

from __future__ import print_function

import os
import sys
import shlex
import subprocess
import multiprocessing as multiprocessing
import signal

from time import time, sleep

ENVFLAGS = {'DEBUG': 'yes'}
REMOTE_ADDR = "0.0.0.0"
REMOTE_PORT = 4001
LOCAL_ADDR = "127.0.0.1"
LOCAL_PORT = 4000
PASSWORD = "this-is-a-password"

addrs = [
    # "http://google.com",
    "https://google.com",
    # "https://ipv6.google.com"
]


# Curl return code:
# https://curl.haxx.se/libcurl/c/libcurl-errors.html
ERR_CODE = dict(
    CURL_SSL_CONNECT_ERR=35,
    CURL_GOT_NOTHING=52,
    CURL_SEND_ERR=55,
)


def test_ok(fmt, *args):
    out = "ok: "
    out += fmt
    print(out  % args)


def test_failed(fmt, *args):
    out = "failed: "
    out += fmt
    print(out % args)


def create_srv_proc(cmd, endtime=None):
    proc = subprocess.Popen(
        shlex.split(cmd), stdin=subprocess.PIPE, stderr=subprocess.PIPE,
        env=ENVFLAGS, cwd="..")

    proc.communicate()

    while True:
        if time() > endtime: # kill process if this tiemout elapsed            
            proc.kill()
            proc.wait()
            sys.exit(1)
            # raise(Exception("timeout"))
        break

    return proc


def call_curl(cmd):
    proc = subprocess.Popen(shlex.split(cmd), env=ENVFLAGS)
    proc.poll()
    if proc.returncode in ERR_CODE.values():
        i = 0
        while True:
            try:
                code = ERR_CODE.values()[i]
                i += 1
            except IndexError:
                break
        raise Exception(ERR_CODE.keys()[i-2])
    elif proc.returncode == 0:
        test_ok("curl")


def call_cmd(cmd):
    proc = subprocess.Popen(
        shlex.split(cmd),
        stdin=subprocess.PIPE, stderr=subprocess.PIPE)
    return proc


timeout = .3

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

    endtime = time() + timeout
    
    proc1 = multiprocessing.Process(target=create_srv_proc, args=(remote, endtime))
    # setattr(proc1, 'daemon', True) # Run server background
    proc1.start()

    proc2 = multiprocessing.Process(target=create_srv_proc, args=(local, endtime))
    # setattr(proc2, 'daemon', True) # Run server background
    proc2.start()

    return proc1, proc2

def test_response(target):
    curl = """curl --socks5 {addr}:{port} {tg} -L -vv""".format(
        addr=LOCAL_ADDR, port=LOCAL_PORT, tg=target)
    proc3 = multiprocessing.Process(target=call_curl, args=(curl,))
    proc3.start()    
    assert proc3.is_alive()


def test():
    p1, p2 = start_servers()

    [test_response(x) for x in addrs]
    
    # p1.join(.5)
    # p2.join(.5)

    sleep(.8)
    
    print("stopped!")


if __name__ == "__main__":
    test()
