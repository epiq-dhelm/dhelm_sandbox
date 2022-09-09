#!/bin/python3

import socket
import select
import sys
import argparse
from argparse import RawTextHelpFormatter

# debug settings
TRACE = 0
DEBUG = 1
ERROR = 2
NONE = 3
level_print = ("TRACE", "DEBUG", "ERROR", "NONE")


SOCKET_TIMEOUT = 30

client_verbose_level = 0
client_debug_level = TRACE

HELP_DESCRIPTION = 'Call apptest server to execute a command\n\n'   +\
        'Available Commands (case insensitive):             \n\n'   +\
        'connectRFE         \t --rfetype ("BOTH")           \n'     +\
        'disconnectRFE      \n'                                     +\
        'startCW            \t --freq (1000)  --power-level (3) \n' +\
        'stopCW             \n'                                     +\
        'startSweep         \t --freq --power-level --steps (20) -- step-width (1000) --waitMS (10000) \n' +\
        'stopSweep          \n'                                     +\
        'peakSearch         \t --span (20) --start-freq (980) --stop-freq (1020)\n' +\
        'setServerDebug     \t --debug-level ("ERROR")      \n'     +\
        'setRFEVerbose      \t --verbose-level (5)          \n'     +\
        'setClientVerbose   \t --verbose-level              \n' 

# prints out the debug printouts based upon the value of the --debug-level args default value
# to change it in the client, you need to change the default value in the args.
def debug_print(level, val1, val2 = None, val3 = None, val4 = None, val5 = None, val6 = None, val7 = None):
    global client_debug_level

    if client_debug_level > level:
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

    string = "[" + level_print[level]  + "] " +  out

    print(string)


# This is the class for the testapp messages.  
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

        if client_verbose_level > 0:
            print(" freq ", freq, " power ", power)

        return resp, freq, power

    def setServerDebug(self, new_debug_level):
        debug_print(TRACE, "setServerDebug")

        cmd = "SETDEBUG " + str(new_debug_level) + " "
        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp

    def setRFEVerbose(self, verbose_level):
        debug_print(TRACE, "setRFEVerbose")

        cmd = "SETVERBOSE " + str(verbose_level) + " "
        self.sendCommand(cmd)

        # wait for a response
        resp, resplist = self.receiveResponse()
        return resp

    def setClientVerbose(self, verbose_level):
        debug_print(TRACE, "setClientVerbose")

        client_verbose_level = verbose_level
        return client_verbose_level

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

def processCommand(rcvd_cmd):
    global test, args

    debug_print(TRACE, "processCommand")

    debug_print(DEBUG, "Command: ", rcvd_cmd)
    cmd = rcvd_cmd.lower()

    if cmd == "connectrfe":
       resp, rfetype = test.connectRFE(args.rfetype) 
       if client_verbose_level > 1:
           print("connectRFE: ", resp, " connected to: ", rfetype)

    elif cmd == "disconnectrfe":
       resp = test.disconnectRFE() 
       if client_verbose_level > 1:
           print("disconnectRFE", resp)

    elif cmd == "startcw":
       resp = test.sendStartCW(args.freq, args.power_level) 
       if client_verbose_level > 1:
           print("StartCW: ", resp)

    elif cmd == "stopcw":
       resp = test.sendStopCW()
       if client_verbose_level > 1:
           print("StopCW: ", resp)

    elif cmd == "startsweep":
       resp = test.sendStartSweep(args.freq, args.power_level, args.steps, args.step_width, args.waitMS)
       if client_verbose_level > 1:
           print("StartSweep: ", resp)

    elif cmd == "stopsweep":
       resp = test.sendStopSweep()
       if client_verbose_level > 1:
           print("StopSweep: ", resp)

    elif cmd == "peaksearch":
       resp, freq, power = test.sendPeakSearch(args.span, args.start_freq, args.stop_freq)
       if client_verbose_level > 1:
           print("PeakSearch: Status: ", resp, "Frequency: ", freq,"Power: ", power)

    elif cmd == "setserverdebug":
       resp = test.setServerDebug(args.debug_level)
       if client_verbose_level > 1:
           print("setServerDebug: ", resp)

    elif cmd == "setrfeverbose":
       resp = test.setRFEVerbose(args.verbose_level)
       if client_verbose_level > 1:
           print("RFE Verbose: ", resp)

    elif cmd == "setclientverbose":
       resp = test.setClientVerbose(args.verbose_level)
       if client_verbose_level > 1:
           print("Client Verbose: ", resp)
    else: 
        raise Exception("Invalid command " + str(cmd))

#---------------------------------------------------------
# Main processing loop
#---------------------------------------------------------
def main(argv=None):
    global test, args, client_debug_level, client_verbose_level

    if argv is None:
        argv = sys.argv

    parser = argparse.ArgumentParser(description= HELP_DESCRIPTION, formatter_class=RawTextHelpFormatter)
    parser.add_argument('--cmd', type=str, default="connectRFE", help='Which command to run')
    parser.add_argument('--rfetype', type=str, default="BOTH", help='Which RFE device to connect to GENERATOR, ANALYZER or BOTH' )
    parser.add_argument('--freq', type=int, default=1000, help='frequency of tone in MHz')
    parser.add_argument('--power', type=int, default=-30, help='power')
    parser.add_argument('--power-level', type=int, default=3, help='Power Level 0-7')
    parser.add_argument('--steps', type=int, default=20, help='Sweep number of steps')
    parser.add_argument('--step-width', type=int, default=1000, help='Sweep step width in Khz')
    parser.add_argument('--waitMS', type=int, default=1000, help='Sweep MS to wait after each change')
    parser.add_argument('--span', type=int, default=20, help='span of Peaksearch in Mhz')
    parser.add_argument('--start-freq', type=int, default=980, help='start freq of Peaksearch in Mhz')
    parser.add_argument('--stop-freq', type=int, default=1020, help='stop freq of Peaksearch in Mhz')
    parser.add_argument('--debug-level', type=int, default=0, help='Debug level to set locally or at server')
    parser.add_argument('--verbose-level', type=int, default=5, help='Verbose level of the RFE device Drivers')


    # get the command line arguments
    args = parser.parse_args(argv[1:])

    client_debug_level = args.debug_level
    client_verbose_level = args.verbose_level

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
