#!/bin/python3

import time
import socket
import sys

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect the socket to the port where the server is listening
server_address = ('localhost', 10000)
print('connecting to ' + server_address[0] + ' port ' + str(server_address[1]))
sock.connect(server_address)

def sendCommand(message):
    print('sending: ', message)
    sock.sendall(message.encode())

def receiveCommand():
    print("receiving")
    data = sock.recv(1024)
    print("received", data)
    return data.decode()

def sendStartCW(frequency, power):
    print("startCW")
    cmd = "STARTCW" + " " + str(frequency) + " " + str(power)
    print(cmd)
    sendCommand(cmd)
   
def sendStopCW():
    print("stopCW")
    cmd = "STOPCW" 
    sendCommand(cmd)

try:
    print("Send Connect Command")
    cmd = "CONNECT"

    sendCommand(cmd)
    resp = receiveCommand()
    print("response", resp)
    if resp == 'TRUE':
        while True: 
            sendStartCW(1000, -30)
            time.sleep(10)

            print("Send STOPCW command")
            sendStopCW()
            time.sleep(10)
finally:
    print('closing socket')
    sock.close()
