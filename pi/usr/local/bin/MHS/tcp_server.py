#!/usr/bin/env python3

import argparse
import logging
import os
import socketserver
import tempfile
import threading
import time

logger = logging.getLogger('sunss')

class SunssState:
    GO_FILE = "/tmp/sunssgo"
    RUNNING_FILE = "/tmp/sunssrunning"
    BUSY_FILE = "/tmp/sunssbusy"

    def __init__(self):
        """Track and control the real SuNSS software, which communicates entirely via files in /tmp."""
        pass

    def isRunning(self):
        return int(os.path.exists(self.RUNNING_FILE))

    def isBusy(self):
        try:
            with open(self.BUSY_FILE, "r") as busyFile:
                state = busyFile.readline()
        except FileNotFoundError:
            return 0

        return int(state)

    def _waitForStart(self):
        """Spin until we know that we are tracking.

        This can involve homing the telescope. Not sure when that happens. Takes a few seconds.
        """

        maxTime = 10
        loopTime = 0.25

        t0 = time.time()
        while True:
            if self.isRunning():
                return True

            t1 = time.time()
            if (t1 - t0) >= maxTime:
                return False
            time.sleep(loopTime)

    def status(self):
        return self.isRunning(), self.isBusy()

    def stop(self):
        try:
            os.remove(self.RUNNING_FILE)
        except FileNotFoundError:
            pass

        return self.status()

    def start(self, ha, dec, time, speed=1):
        """Request that we start tracking at a given position.

        Args
        ----
        ha : float
         Hour angle, degrees
        dec : float
         Declination, degrees
        time : float
         unix seconds for the HA. Can use 0.
        speed : int
         acceleration factor. Increase from 1 to make time go faster.

        Note that the file needs to be created and filled in atomically, like "echo foo > file".

        """

        self.stop()
        with tempfile.NamedTemporaryFile(mode='wt', dir='/tmp', delete=False) as gofile:
            print(f'{ha} {dec} {time} {speed}', file=gofile)
            tempname = gofile.name
        os.rename(tempname, self.GO_FILE)
        self._waitForStart()
        return self.status()

class SunssRequestHandler(socketserver.BaseRequestHandler):
    def setup(self):
        self.sunssState = self.server.sunssState

    def handle(self):
        rawCmd = str(self.request.recv(1024), 'latin-1')
        cmd = rawCmd.split()
        print("cmd: ", cmd)

        cmdName = cmd[0]
        if cmdName == 'track':
            _, ha, dec, time, speed = cmd
            ret = self.sunssState.start(ha, dec, time, speed)
        elif cmdName == 'stop':
            ret = self.sunssState.stop()
        elif cmdName == 'status':
            ret = self.sunssState.status()

        response = f'{ret[0]} {ret[1]}\n'
        self.request.sendall(response.encode('latin-1'))

class SunssServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    def __init__(self, *kl, sunssState=None, **kv):
        self.sunssState = sunssState
        super().__init__(*kl, **kv)
        self.allow_reuse_address = True
        self.name = 'sunss'

def run():
    state = SunssState()
    ip, port = '', 1024
    with SunssServer((ip, port), SunssRequestHandler, sunssState=state) as server:
        # Start a thread with the server -- that thread will then start one
        # more thread for each request
        server_thread = threading.Thread(target=server.serve_forever)

        # Exit the server thread when our thread terminates
        server_thread.daemon = True
        print("Server loop starting in thread:", server_thread.name)
        server_thread.start()
        server_thread.join()
        print("Server loop done in thread:", server_thread.name)

def main(argv=None):
    if isinstance(argv, str):
        import shlex
        argv = shlex.split(argv)

    parser = argparse.ArgumentParser()
    # args = parser.parse(argv)

    run()

if __name__ == "__main__":
    main()
