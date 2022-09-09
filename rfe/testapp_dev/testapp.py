#!/bin/python3

import socket
import select
import sys
import argparse

# debug settings
TRACE = 0
DEBUG = 1
ERROR = 2
NONE = 3
level_print = ("TRACE", "DEBUG", "ERROR", "NONE")

SOCKET_TIMEOUT = 30

debug_level = TRACE

def debug_print(level, val1, val2 = None, val3 = None, val4 = None, val5 = None, val6 = None, val7 = None):
    global debug_level

    if debug_level > level:
        return

    out = str(val1)
    if val2 != None:
        out = out + " " + str(val2)
    if val3 != None:
        out = out + " " + str(val3)
    if val4 != None:
        out = out + " " + str(val4)
    if val5 != None:
        out = out + " " + str(val5)
    if val6 != None:
        out = out + " " + str(val6)
    if val7 != None:
        out = out + " " + str(val7)

    print("[", level_print[level], "] ", out)


class TestApp():

    def sendCommand(self, message):
        debug_print(TRACE, 'sending: ', message)

        self.sock.sendall(message.encode())

    def receiveData(self):
        debug_print(TRACE, "receiveData")

        try:
            data = self.sock.recv(1024)

        except socket.timeout:
            raise Exception("Client timed out waiting for a response")

        return data.decode()

    def receiveResponse(self):
        debug_print(TRACE, "receiveResponse")

        response = self.receiveData()
        resplist = response.split()
        length = len(resplist)

        if length > 0:
            cmd = resplist.pop(0)
        else:
            cmd = "ERROR"
            response = "Client received data from server with 0 length"

        if cmd == "ERROR":
            raise Exception("Server returned an error: " + response)

        debug_print(DEBUG, "Response: len: ", length, "cmd: ", cmd)

        return cmd, resplist

    def sendStartCW(self, frequency, power_level):
        debug_print(TRACE, "startCW")
        cmd = "STARTCW" + " " + str(frequency) + " " + str(power_level) + " "

        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp
       
    def sendStopCW(self):
        debug_print(TRACE, "stopCW")
        cmd = "STOPCW " 
        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp

    def sendStartSweep(self, start_freq, stop_freq, power_level, steps, waitMS ):
        debug_print(TRACE, "startSweep")
        cmd = "STARTSWEEP" + " " + str(start_freq) + " " + str(stop_freq) + " " + str(power_level) + " " + str(steps) + " " +  str(waitMS) + " "

        debug_print(DEBUG, cmd)
        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp


    def sendStopSweep(self):
        debug_print(TRACE, "stopSweep")
        cmd = "STOPSWEEP " 
        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp

    def sendPeakSearch(self, span, start_scan, stop_scan):
        debug_print(TRACE, "sendPeakSearch")

        freq = 0
        power = 0

        cmd = "PEAKSEARCH " + str(span) + " " + str(start_scan) + " " + str(stop_scan)   + " "

        self.sendCommand(cmd)

        #wait for a response
        resp, resplist = self.receiveResponse()

        freq = float(resplist.pop(0))
        power = float(resplist.pop(0))

        debug_print(DEBUG, "freq ", freq, " power ", power)

        return resp, freq, power

    def setDebug(self, debug_level):
        debug_print(TRACE, "setDebug")

        cmd = "SETDEBUG " + str(debug_level) + " "
        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp

    def setVerbose(self, verbose_level):
        debug_print(TRACE, "setVerbose")

        cmd = "SETVERBOSE " + str(verbose_level) + " "
        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp

    def connectRFE(self, rfetype):
        debug_print(TRACE, "connectRFE")

        cmd = "CONNECTRFE " + rfetype + " "

        self.sendCommand(cmd)

        resp, cmdlist = self.receiveResponse()
        
        if len(cmdlist) != 0:
            rfetype = cmdlist.pop(0)
        else: 
            rfetype = "NONE"

        return resp, rfetype

    def disconnectRFE(self):
        debug_print(TRACE, 'disconnectRFE')

        cmd = "DISCONNECTRFE " 

        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp

    def connectServer(self, server_address = 'localhost'):
        debug_print(TRACE, 'connectServer')

        # Create a TCP/IP socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    

        # Connect the socket to the port where the server is listening
        self.server_address = (server_address, 10000)
        debug_print(DEBUG, 'connecting to ' + self.server_address[0] + ' port ' + str(self.server_address[1]))
        self.sock.connect(self.server_address)
        self.sock.settimeout(SOCKET_TIMEOUT)

    def disconnectServer(self):
        debug_print(TRACE, 'disconnectServer')

        self.sock.close()

def processCommand(cmd):
    global test, args

    debug_print(TRACE, "processCommand")

    debug_print(DEBUG, "Command: ", cmd)

    if cmd == "connectRFE":
       resp, rfetype = test.connectRFE(args.rfetype) 
       print("connectRFE: ", resp, " connected to: ", rfetype)

    elif cmd == "disconnectRFE":
       resp = test.disconnectRFE() 
       print("disconnectRFE", resp)

    elif cmd == "StartCW":
       resp = test.sendStartCW(args.freq, args.power_level) 
       print("StartCW: ", resp)

    elif cmd == "StopCW":
       resp = test.sendStopCW()
       print("StopCW: ", resp)

    elif cmd == "StartSweep":
       resp = test.sendStartSweep(args.freq, args.power_level, args.steps, args.step_width, args.waitMS)
       print("StartSweep: ", resp)

    elif cmd == "StopSweep":
       resp = test.sendStopSweep()
       print("StopSweep: ", resp)

    elif cmd == "PeakSearch":
       resp, freq, power = test.sendPeakSearch(args.span, args.start_freq, args.stop_freq)
       print("PeakSearch: Status: ", resp, "Frequency: ", freq,"Power: ", power)

    elif cmd == "StopSweep":
       resp = test.sendStopSweep()
       print("StopSweep: ", resp)

    elif cmd == "setDebug":
       resp = test.setDebug(debug_level)
       print("setDebug: ", resp)

    elif cmd == "setVerbose":
       resp = test.setVerbose(args.verbose_level)
       print("Verbose: ", resp)
    else: 
        raise Exception("Invalid command " + str(cmd))

#---------------------------------------------------------
# Main processing loop
#---------------------------------------------------------
def main(argv=None):
    global test, args, debug_level

    if argv is None:
        argv = sys.argv

    parser = argparse.ArgumentParser(description='Call apptest server to execute a command')
    parser.add_argument('--cmd', type=str, default="connectRFE", help='Which command to run')
    parser.add_argument('--rfetype', type=str, default="BOTH", help='Which RFE device to connect to GENERATOR, ANALYZER or BOTH' )
    parser.add_argument('--freq', type=int, default=1000000000, help='center frequency')
    parser.add_argument('--power', type=int, default=-30, help='power')
    parser.add_argument('--power-level', type=int, default=3, help='Power Level 0-7')
    parser.add_argument('--steps', type=int, default=20, help='Sweep number of steps')
    parser.add_argument('--step-width', type=int, default=1000, help='Sweep step width in Khz')
    parser.add_argument('--waitMS', type=int, default=1000, help='Sweep MS to wait after each change')
    parser.add_argument('--span', type=int, default=20, help='span of Peaksearch in Mhz')
    parser.add_argument('--start-freq', type=int, default=980, help='start freq of Peaksearch in Mhz')
    parser.add_argument('--stop-freq', type=int, default=1020, help='stop freq of Peaksearch in Mhz')
    parser.add_argument('--debug-level', type=str, default="NONE", help='Debug level to set locally or at server')
    parser.add_argument('--verbose-level', type=str, default=0, help='Debug level to set locally or at server')

    # get the command line arguments
    args = parser.parse_args(argv[1:])

    # figure out the debug level they want
    debug_level = 999 
    for idx, name in enumerate(level_print):
        if name == args.debug_level:
            debug_level = idx
            break
    if debug_level == 999:
        raise Exception("Invalid debug level " + args.debug_level)

    # create the test object
    test = TestApp()
    try:
        # connect to the test server via TCP
        test.connectServer()

        # process the command they sent in
        processCommand(args.cmd)

    except Exception as obEx:
        print("*Error: " + str(obEx))
        return 1

if __name__ == '__main__':
      sys.exit(main())
