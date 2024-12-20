#!/bin/python3

import sys

import tkinter as tk
from tkinter import ttk
from tkinter import *
from tkinter.messagebox import Message

import time
import math

import RFExplorer
print(RFExplorer.__file__)
from RFExplorer import RFE_Common



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

init_vals = [
    "997.0",
    "2",
    "10",
    "1000",
    "1000",
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
DEBUG = TRUE

root = Tk(className='HappyName')
root.wm_title('RFE Signal Generator')
current_frame = Frame(root)

def enterhandler(e):
    global current_state

    if DEBUG:
        print("enterhandler", current_state, e)

    if current_state == "get_port":
        go()
    elif current_state == "continuous_wave":
        continuous_power_on()
    elif current_state == "frequency_sweep":
        start_sweep()


def power_off():
    global on_button

    if DEBUG:
        print("power off")
    on_button.configure(bg = "#f0f0f0")
    
    objRFE.SendCommand("CP0")

def continuous_power_on():
    global cw_freq, cw_power, current_color, on_button

    current_color = "red"
    on_button.configure(bg= "red")
    
    if DEBUG:
        print("power on")

    frequency = cw_freq.get()
    if DEBUG:
        print("frequency str ", frequency)

    floatfreq = float(frequency) * 1000

    intfreq = int(floatfreq)

    if DEBUG:
        print("float freq ", floatfreq, "int freq ", intfreq)

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

    objRFE.SendCommand(cmd)

def continuous_wave():
    global current_frame, cw_freq, cw_power, current_state, on_button
    
    current_state = "continuous_wave"

    if DEBUG:
        print("continuous wave")

    current_frame.destroy()
    root.geometry('550x200')
    current_frame = Frame(root)

    label = Label(master=current_frame, text="Frequency MHz: ", font=("Arial", 16))
    label.grid(row = 0, column = 0, padx = 20, pady = 10)
    cw_freq = Entry(master=current_frame, width = 10, font=("Ariel", 16))
    cw_freq.insert(0, "1000.0")
    cw_freq.focus_set()
    cw_freq.grid(row = 0, column = 1, padx = 20, pady = 10)

    label2 = Label(master=current_frame, text="Power (0-7) : ", font=("Arial", 16))
    label2.grid(row = 1, column = 0, padx = 20, pady = 10)
    cw_power = Entry(master=current_frame, width = 10, font=("Ariel", 16))
    cw_power.insert(0, "2")
    cw_power.grid(row = 1, column = 1, padx = 20, pady = 10)

    on_button = Button(current_frame, text = 'Power On') 
#    on_button.configure(font=("Arial", 16), command = lambda name= on_button: continuous_power_on(name) )
    on_button.configure(font=("Arial", 16), command = continuous_power_on )
    on_button.grid(row = 2, column = 0, sticky = W, padx = 20, pady = 10)
    Button(current_frame, text = 'Power Off', font=("Arial", 16), command = power_off).grid(row = 2, column = 1, sticky = W, padx = 20, pady = 10)
    Button(current_frame, text = 'Back', font=("Arial", 16), command = top_window).grid(row = 2, column = 2, sticky = W, padx = 20, pady = 10)
    current_frame.pack(fill="both", expand=True)

def start_sweep():
    global user_entry, current_color, on_button

    if DEBUG:
        print("start sweep")
    
    on_button.configure(bg= "red")
    current_color = "red"

    frequency = user_entry[0].get()

    if DEBUG:
        print("frequency str ", frequency)

    floatfreq = float(frequency) * 1000

    intfreq = int(floatfreq)

    if DEBUG:
        print("float freq ", floatfreq, "int freq ", intfreq)

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
 
    objRFE.SendCommand(cmd)
    
def clear():
    if DEBUG:
        print("clear")

    for idx, text in enumerate(labels):
        user_entry[idx].delete(0, END)



def frequency_sweep():
    global current_frame, user_entry, current_state, on_button

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

        user_entry[idx].insert(0, init_vals[idx])

        if idx == 0:
            user_entry[0].focus_set()

        # Use the grid geometry manager to place the Label and
        # Entry widgets in the row whose index is idx
        label.grid(row=idx, column=0, padx=10, pady=10, sticky="e")

        user_entry[idx].grid(row=idx, column=1, padx=10, pady=10)

    on_button = Button(current_frame, text = 'Start Sweep') 
    on_button.configure(font=("Arial", 16), command = start_sweep )
    on_button.grid(row = idx+1, column = 0, sticky = W, padx = 10, pady = 10)
    Button(current_frame, text = 'Stop Sweep', font=("Arial", 16), command = power_off).grid(row = idx+1, column = 1, sticky = NSEW, padx = 10, pady = 10)
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



def exit_program():
    global port
    if DEBUG:
        print("exit program")
    port.close()
    exit()

# The user hit enter after picking a port
def go():
    global app, user_entry
    portno = int(user_entry.get())

    # verify the entered number
    if portno >= len(objRFE.m_arrValidCP2102Ports) or portno < 0:
        showinfo("Error", "Error: Invalid Port Number " + str(portno) , 2)
        return

    else:
        PortName = objRFE.m_arrValidCP2102Ports[portno].device
        if DEBUG:
            print("Selected name", PortName)

    #Connect to available port
    objRFE.ConnectPort(PortName, BAUDRATE)
    if DEBUG:
        print("Connected...")

    # remove the labels for the various ports so we can display that we are resetting 
    # the device
    for obj in label:
        obj.after(10, obj.destroy())
    
    user_label2 = Label(current_frame, text= "Resetting device ...", font=("Arial", 16))
    user_label2.grid(row=1, column=0, padx=10, pady=10, sticky="w")
    current_frame.pack(fill="both", expand=True)

    # if configured reset the device 
    if RESET:
        if DEBUG:
            print("Resetting device...")

        #Reset the unit to start fresh
        objRFE.SendCommand("r")

        #Wait for unit to notify reset completed
        while(objRFE.IsResetEvent):
            pass

        #Wait for unit to stabilize
        # for some reason we need to do a messagebox to get this to work.
        showinfo("Info", "Resetting Device", None)
        time.sleep(4)

    top_window()


# Display the window to let the user select the port desired and pick one
def GetPort():
    global app, current_frame, user_entry, current_state

    current_state = "get_port"

    # delete one if it exists
    current_frame.destroy()
    frame = Frame(root)
    current_frame = frame

    # for some reason when it only goes through the for loop once if it 
    # deletes something.  So this while loop fixes that.
    done = False
    while (not done):
        deleted = False

        for objPort in objRFE.m_arrValidCP2102Ports:
            if (objPort.device == "/dev/ttyAMA0"):
                deleted = True
                objRFE.m_arrValidCP2102Ports.remove(objPort)

            if (objPort.device == "/dev/ttySKIQ_UART0"):
                deleted = True
                objRFE.m_arrValidCP2102Ports.remove(objPort)

            if (objPort.device == "/dev/ttySKIQ_UART1"):
                deleted = True
                objRFE.m_arrValidCP2102Ports.remove(objPort)

            if (objPort.device == "/dev/ttyS4") :
                deleted = True
                objRFE.m_arrValidCP2102Ports.remove(objPort)

        if deleted == False:
            done = True

    # see if we have more than one valid port, if so we need to display a window with the
    # choices
    if len(objRFE.m_arrValidCP2102Ports) > 1:

        # print the title of the window
        title = Label(current_frame, text= "Available Ports:", font=("Arial", 16))
        title.grid(row=0, column=1, padx=10, pady=10, sticky="ew")

        # build labels that contain each available port
        ctr = 0
        for objPort in objRFE.m_arrValidCP2102Ports:
            tmp_txt = str(ctr) + "     " +  objPort.device
            label.append(Label(current_frame, text=tmp_txt, font=("Arial", 16)))
            # Entry widgets in the row whose index is idx
            label[ctr].grid(row=ctr+1, column=0, padx=10, pady=10, sticky="w")
            ctr += 1
        

        # Display the label that tells the user what to do
        user_label = Label(current_frame, text= "Enter Port Number:", font=("Arial", 16))
        user_label.grid(row=ctr+2, column=0, padx=10, pady=10, sticky="w")

        # build the Entry box for them to enter it.
        user_entry = Entry(current_frame, width=5, font=("Ariel", 16))
        user_entry.grid(row=ctr+2, column=1, padx=10, pady=10)

        # put the cursor inside the entry box
        user_entry.focus_set()

        # display it
        current_frame.pack(fill="both", expand=True)

    # if there is exactly one port no need to select it, connect to it
    elif len(objRFE.m_arrValidCP2102Ports) == 1:
        PortName = objRFE.m_arrValidCP2102Ports[0].device
        if DEBUG:
            print("Selected name", PortName)

        #Connect to available port
        objRFE.ConnectPort(PortName, BAUDRATE)
        if DEBUG:
            print("Connected...")

        # if configured to reset, reset it and display that on the window.
        if RESET:
            user_label = Label(current_frame, text= "Resetting device ...", 
                    font=("Arial", 16))
            user_label.grid(row=2, column=0, padx=10, pady=10, sticky="w")
            
            # display it
            current_frame.pack(fill="both", expand=True)

            #Reset the unit to start fresh
            objRFE.SendCommand("r")

            #Wait for unit to notify reset completed
            while(objRFE.IsResetEvent):
                pass

            #Wait for unit to stabilize
            showinfo("Info", "Resetting Device", None)
            time.sleep(4)

        top_window()
        """
        # build the main window
        content, frame = build_window()

        # initialize the main window
        app.init_window(content) 
        """

    else: #no valid ports
        showinfo("Error", "No Valid Ports Found", 5)
        
        #remove this window
        root.destroy()

        # leave
        cleanup()
    
    return True

# used to display a message box describing the info
def showinfo(msgtitle, text, waittime):
    newroot = Tk()


    if waittime != None:
        newroot.after(waittime * 1000, newroot.destroy)
    else:
        # if waittime is None then we just quickly display the box and move on
        newroot.after(10, newroot.destroy)

    # make this window small so it can't be seen under the message box
    # there is no way to place the message box itself so we place the window
    newroot.geometry('10x10+200+200')

    # create and show the message box
    try:
        Message(title=msgtitle, message=text, master=newroot).show()
    except TclError:
        pass

# if something goes wrong go here
def cleanup():
    # kill the main window
    root.destroy

    # close RFE
    objRFE.Close()

    # leave
    exit()

root.geometry('550x350+100+200')
root.resizable(True, True)
root.bind('<Return>', enterhandler)

# list of port labels
label = []

objRFE = RFExplorer.RFECommunicator()

objRFE.GetConnectedPorts()

objRFE.VerboseLevel = 10

GetPort()

root.mainloop()

objRFE.Close()

