#!/bin/python3

import sys
from tkinter import *

import time
import math

import serial
import serial.tools.list_ports

"""
Power Table:
    0   -35.2
    1   -33.2
    2   -29.2
    3   -26.2
    4   -5.7
    5   -2.7
    6    0.3
    7   +2.8


"""

#---------------------------------------------------------
# global variables and initialization
#---------------------------------------------------------

# List of field labels
labels = [
    "Start Freq MHz:",
    "Power Level (0 - 7):",
    "Number of Steps:",
    "Freq Step in KHz:",
    "Step Time in ms:",
]

user_entry = []
usb_port = ''
current_state = ""

SERIALPORT = None    #serial port identifier, use None to autodetect
BAUDRATE = 500000

SPAN_SIZE_MHZ = 50           #Initialize settings
START_SCAN_MHZ = 50
STOP_SCAN_MHZ = 960
RESET = True
DEBUG = False

root = Tk(className='HappyName')
root.wm_title('RFE Signal Generator')
current_frame = Frame(root)

def enterhandler(e):
    global current_state

    if DEBUG:
        print("enterhandler", current_state, e)

    if current_state == "getport":
        go()
        current_state = ''
"""
    if current_state == "continuous_wave":
        continuous_power_on()

    if current_state == "frequency_sweep":
        start_sweep()
"""

def SendCommand(cmd):
    global port

    sCompleteCommand = "#" + chr(len(cmd) + 2) + cmd
    bytestr = sCompleteCommand.encode('utf-8')
    length = port.write(bytestr)
    if DEBUG:
        print(length, "RFE Command: #(" + str(len(sCompleteCommand)) + ")" + cmd + " [" + " ".join("{:02X}".format(ord(c)) for c in sCompleteCommand) + "]")


def power_off(on_button):
    if DEBUG:
        print("power off")
    on_button.configure(bg = "#f0f0f0")
    
    SendCommand("CP0")

def continuous_power_on(on_button):
    global cw_freq, cw_power, current_color

    current_color = "red"
    on_button.configure(bg= "red")
    
    if DEBUG:
        print("power on")

    frequency = cw_freq.get()
    intfreq = int(frequency) * 1000

    if DEBUG:
        print("frequency ", intfreq)

    power = int(cw_power.get())

    if power > 7 or power < 0:
        print("invalid power", power, "changing to 7")
        power = 7
    
    if power > 3:
        config_power = power - 4
        highpower = 1
    else:
        config_power = power
        highpower = 0

    if DEBUG:
        print("power", power)


    cmd = "C3-F:" + "{:07d}".format(intfreq) + ","+ str(highpower)+ "," + str(config_power)

    SendCommand(cmd)

def continuous_wave():
    global current_frame, cw_freq, cw_power, current_state
    
    current_state = "continuous_wave"

    if DEBUG:
        print("continuous wave")

    current_frame.destroy()
    root.geometry('550x200')
    current_frame = Frame(root)

    label = Label(master=current_frame, text="Frequency MHz: ", font=("Arial", 16))
    label.grid(row = 0, column = 0, padx = 20, pady = 10)
    cw_freq = Entry(master=current_frame, width = 10, font=("Ariel", 16))
    cw_freq.focus_set()
    cw_freq.grid(row = 0, column = 1, padx = 20, pady = 10)

    label2 = Label(master=current_frame, text="Power (0-7) : ", font=("Arial", 16))
    label2.grid(row = 1, column = 0, padx = 20, pady = 10)
    cw_power = Entry(master=current_frame, width = 10, font=("Ariel", 16))
    cw_power.grid(row = 1, column = 1, padx = 20, pady = 10)

    on_button = Button(current_frame, text = 'Power On') 
    on_button.configure(font=("Arial", 16), command = lambda name= on_button: continuous_power_on(name) )
    on_button.grid(row = 2, column = 0, sticky = W, padx = 20, pady = 10)
    Button(current_frame, text = 'Power Off', font=("Arial", 16), command = lambda name = on_button: power_off(name)).grid(row = 2, column = 1, sticky = W, padx = 20, pady = 10)
    Button(current_frame, text = 'Back', font=("Arial", 16), command = top_window).grid(row = 2, column = 2, sticky = W, padx = 20, pady = 10)
    current_frame.pack(fill="both", expand=True)

def start_sweep(on_button):
    global user_entry, current_color

    if DEBUG:
        print("start sweep")
    
    on_button.configure(bg= "red")
    current_color = "red"

    frequency = int(user_entry[0].get())
    intfreq = int(frequency) * 1000

    power = int(user_entry[1].get())
    if power > 7 or power < 0:
        print("invalid power", power, "changing to 7")
        power = 7

    if power > 3:
        config_power = power - 4
        highpower = 1
    else:
        config_power = power
        highpower = 0
    
    steps = user_entry[2].get()

    step_width = user_entry[3].get()

    step_time = user_entry[4].get()

    cmd = "C3-F:" + "{:07d}".format(intfreq) + "," + str(highpower) + "," + str(config_power) + "," + "{:04d}".format(int(steps)) + "," + "{:07d}".format(int(step_width)) + "," + "{:05d}".format(int(step_time))
 
    SendCommand(cmd)
    
def clear():
    if DEBUG:
        print("clear")

    for idx, text in enumerate(labels):
        user_entry[idx].delete(0, END)



def frequency_sweep():
    global current_frame, user_entry, current_state

    current_state = "frequency_sweep"

    current_frame.destroy()
    root.geometry('750x350')
    frame = Frame(root)
    current_frame = frame

    user_entry = []

    # Loop over the list of field labels
    for idx, text in enumerate(labels):

        # Create a Label widget with the text from the labels list
        label = Label(current_frame, text=text, font=("Arial", 16))

        # Create an Entry widget
        user_entry.append(Entry(current_frame, width=20, font=("Ariel", 16)))

        if idx == 0:
            user_entry[0].focus_set()

        # Use the grid geometry manager to place the Label and
        # Entry widgets in the row whose index is idx
        label.grid(row=idx, column=0, padx=10, pady=10, sticky="e")

        user_entry[idx].grid(row=idx, column=1, padx=10, pady=10)

    on_button = Button(current_frame, text = 'Start Sweep') 
    on_button.configure(font=("Arial", 16), command = lambda name= on_button: start_sweep(name) )
    on_button.grid(row = idx+1, column = 0, sticky = W, padx = 10, pady = 10)
    Button(current_frame, text = 'Stop Sweep', font=("Arial", 16), command = lambda name = on_button: power_off(name)).grid(row = idx+1, column = 1, sticky = NSEW, padx = 10, pady = 10)
    Button(current_frame, text = 'Clear', font=("Arial", 16), command = clear).grid(row = idx+1, column = 2, sticky = NSEW, padx = 10, pady = 10)
    Button(current_frame, text = 'Back', font=("Arial", 16), command = top_window).grid(row = idx+1, column = 3, sticky = NSEW, padx = 10, pady = 10)
    current_frame.pack(fill="both", expand=True)

def top_window():
    global current_frame, root
    
    current_state = ""
    current_frame.destroy()
    root.geometry('650x100')
    current_frame = Frame(root)
    Button(current_frame, text = 'Frequency Sweep', font=("Arial", 16), command = frequency_sweep).grid(row = 1, column = 0, sticky = W, padx = 20, pady = 10)
    Button(current_frame, text = 'Continuous Wave', font=("Arial", 16), command = continuous_wave).grid(row = 1, column = 1, sticky = W, padx = 20, pady = 10)
    Button(current_frame, text = 'Exit', font=("Arial", 16), command = exit_program).grid(row = 1, column = 3, sticky = W, padx = 20, pady = 10)
    current_frame.pack(fill="both", expand=True)





def go():
    global ports, port
    portno = int(user_entry[0].get())

    if portno >= len(ports) or portno < 0:
        print("Error: Invalid Port Number", portno)
        GetPort()
    else:
        port.baudrate = BAUDRATE
        port.port = ports[portno].device
        port.timeout = 5.0
        port.open()

        if DEBUG:
            print("Selected name", port.port)

    if RESET:
        if DEBUG:
            print("Resetting device...")

        #Reset the unit to start fresh
        SendCommand("r")

        #Wait for unit to stabilize
        time.sleep(4)

    if DEBUG:
        print("Connected...")

    top_window()

def GetPort():
    global ports, port, current_frame, current_state 

    current_frame.destroy()
    frame = Frame(root)
    current_frame = frame
    current_state = "getport"

    title = Label(current_frame, text= "Available Ports:", font=("Arial", 16))
    title.grid(row=0, column=1, padx=10, pady=10, sticky="ew")


    for idx, objPort in enumerate(ports):
        tmp_txt = str(idx) + "     " +  objPort.device
        label = Label(current_frame, text=tmp_txt, font=("Arial", 16))
        # Entry widgets in the row whose index is idx
        label.grid(row=idx+1, column=0, padx=10, pady=10, sticky="w")

    user_label = Label(current_frame, text= "Enter Port Number:", font=("Arial", 16))
    user_label.grid(row=idx+2, column=0, padx=10, pady=10, sticky="w")
    user_entry.append(Entry(current_frame, width=5, font=("Ariel", 16)))
    user_entry[0].grid(row=idx+2, column=1, padx=10, pady=10)
    user_entry[0].focus_set()

    current_frame.pack(fill="both", expand=True)

def connect():
    global ports, port

    if DEBUG:
        print("Connecting")

    label = Label(master=current_frame, text="Connecting... ", font=("Arial", 16))
    label.place(relx = 0.5, rely = 0.5, anchor = 'center')
    current_frame.pack(fill="both", expand=True)

    for idx, objPort in enumerate(ports):
        if (objPort.device == "/dev/ttyAMA0" or objPort.device == "/dev/ttyS4"):
            ports.remove(objPort)

    #if we have more than one port we need to determine which one.
    if (len(ports) > 1):
        GetPort()
    #Connect to available port
    else:
        port.baudrate = BAUDRATE
        port.port = ports[0].device
        port.timeout = 5.0
        port.open()
#        port = serial.Serial(ports[0].device, 500000, timeout=5.0)
 
        if RESET:
            if DEBUG:
                print("Resetting device...")

            #Reset the unit to start fresh
            SendCommand("r")

            #Wait for unit to stabilize
            time.sleep(4)

        if DEBUG:
            print("Connected...")

        top_window()

def exit_program():
    global port
    if DEBUG:
        print("exit program")
    port.close()
    exit()

ports = list(serial.tools.list_ports.comports())
port = serial.Serial()

root.geometry('550x350+100+200')
root.resizable(True, True)
root.bind('<Return>', enterhandler)

connect()

root.mainloop()
