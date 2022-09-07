#!/bin/python3

import socket
import sys

# debug settings
NONE = 0
TRACE = 1
DEBUG = 2
ERROR = 3
level_print = ("NONE", "TRACE", "DEBUG", "ERROR")


debug_level = TRACE

def debug_print(level, val1, val2 = None, val3 = None, val4 = None):
    if debug_level > level:
        return

    out = str(val1)
    if val2 != None:
        out = out + " " + str(val2)
    if val3 != None:
        out = out + " " + str(val3)
    if val4 != None:
        out = out + " " + str(val4)

    print("[", level_print[level], "] ", out)


class TestApp():

    def sendCommand(self, message):
        debug_print(TRACE, 'sending: ', message)

        self.sock.sendall(message.encode())

    def receiveCommand(self):
        debug_print(TRACE, "receiving")

        data = self.sock.recv(1024)
        return data.decode()

    def receiveResponse(self):
        debug_print(TRACE, "receiveResponse")

        response = self.receiveCommand()
        resplist = response.split()
        length = len(resplist)
        cmd = resplist.pop(0)

        if cmd == "ERROR":
            raise Exception("Server returned an error: " + resplist)

        debug_print(DEBUG, "Response: len: ", length, "cmd: ", cmd)

        return cmd, resplist

    def sendStartCW(self, frequency, power):
        debug_print(TRACE, "startCW")
        cmd = "STARTCW" + " " + str(frequency) + " " + str(power) + " "

        self.sendCommand(cmd)

        # wait for a response
        self.receiveResponse()

       
    def sendStopCW(self):
        debug_print(TRACE, "stopCW")
        cmd = "STOPCW " 
        self.sendCommand(cmd)

        # wait for a response
        self.receiveResponse()

    def sendStartSweep(self, start_freq, stop_freq, power, steps, step_size, waitMS ):
        debug_print(TRACE, "startSweep")
        cmd = "STARTSWEEP" + " " + str(start_freq) + " " + str(stop_freq) + " " + str(power) + " " + str(steps) + " " + str(step_size) + " " + str(waitMS) + " "

        debug_print(DEBUG, cmd)
        self.sendCommand(cmd)
        # wait for a response
        self.receiveResponse()


    def sendStopSweep(self):
        debug_print(TRACE, "stopSweep")
        cmd = "STOPSWEEP " 
        self.sendCommand(cmd)

        # wait for a response
        self.receiveResponse()

    def sendPeakSearch(self, span, start_scan, stop_scan):
        debug_print(TRACE, "sendPeakSearch")

        freq = 0
        power = 0

        cmd = "PEAKSEARCH " + str(span) + " " + str(start_scan) + " " + str(stop_scan)   + " "

        self.sendCommand(cmd)

        cmd, resplist = self.receiveResponse()

        freq = float(resplist.pop(0))
        power = float(resplist.pop(0))

        debug_print(DEBUG, "freq ", freq, " power ", power)

        return freq, power

    def setDebug(self, debug_level):
        debug_print(TRACE, "setDebug")

        cmd = "SETDEBUG " + str(debug_level) + " "
        self.sendCommand(cmd)

        # wait for a response
        self.receiveResponse()

    def setVerbose(self, verbose_level):
        debug_print(TRACE, "setVerbose")

        cmd = "SETVERBOSE " + str(verbose_level) + " "
        self.sendCommand(cmd)

        # wait for a response
        self.receiveResponse()

    def connectRFE(self, rfetype):
        debug_print(TRACE, "connectRFE")

        cmd = "CONNECTRFE " + rfetype + " "

        self.sendCommand(cmd)

        cmd, resp = self.receiveResponse()

        return resp.pop(0)

    def disconnectRFE(self, rfetype):
        debug_print(TRACE, 'disconnectRFE')

        cmd = "DISCONNECTRFE " + rfetype + " "

        self.sendCommand(cmd)

        # wait for a response
        self.receiveResponse()

    def connectServer(self, server_address = 'localhost'):
        debug_print(TRACE, 'connectServer')

        # Create a TCP/IP socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # Connect the socket to the port where the server is listening
        self.server_address = (server_address, 10000)
        debug_print(DEBUG, 'connecting to ' + self.server_address[0] + ' port ' + str(self.server_address[1]))
        self.sock.connect(self.server_address)

    def disconnectServer(self):
        debug_print(TRACE, 'disconnectServer')

        self.sock.close()

