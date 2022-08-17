#!/bin/python3

#---------Imports

import time
import RFExplorer
print(RFExplorer.__file__)
from RFExplorer import RFE_Common
import math
import array as arr



from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

import tkinter as tk
from tkinter import ttk
from tkinter import *

import numpy as np

import matplotlib.pyplot as plt
import matplotlib.animation as animation

#---------End of imports



from tkinter import Frame,Label,Entry,Button

DEBUG = False

SERIALPORT = "/dev/ttyUSB0"    #serial port identifier, use None to autodetect
BAUDRATE = 500000

SPAN_SIZE_MHZ = 50           #Initialize settings
START_SCAN_MHZ = 975
STOP_SCAN_MHZ = START_SCAN_MHZ + SPAN_SIZE_MHZ
SWEEPPOINTS = 112
SWEEPDATA = (SWEEPPOINTS + 1) * 16
RESET = True

def ControlSettings(objAnalazyer):
    """This functions check user settings
    """
    SpanSizeTemp = None
    StartFreqTemp = None
    StopFreqTemp =  None

    #print user settings
    if DEBUG:
        print("User settings:" + "Span: " + str(SPAN_SIZE_MHZ) +"MHz"+  " - " + "Start freq: " + str(START_SCAN_MHZ) +"MHz"+" - " + "Stop freq: " + str(STOP_SCAN_MHZ) + "MHz")

    #Control maximum Span size
    if(objAnalazyer.MaxSpanMHZ <= SPAN_SIZE_MHZ):
        print("Max Span size: " + str(objAnalazyer.MaxSpanMHZ)+"MHz")
    else:
        objAnalazyer.SpanMHZ = SPAN_SIZE_MHZ
        SpanSizeTemp = objAnalazyer.SpanMHZ
    if(SpanSizeTemp):
        #Control minimum start frequency
        if(objAnalazyer.MinFreqMHZ > START_SCAN_MHZ):
            print("Min Start freq: " + str(objAnalazyer.MinFreqMHZ)+"MHz")
            objRFE.Close()
            exit()
        else:
            objAnalazyer.StartFrequencyMHZ = START_SCAN_MHZ
            StartFreqTemp = objAnalazyer.StartFrequencyMHZ
        if(StartFreqTemp):
            #Control maximum stop frequency
            if(objAnalazyer.MaxFreqMHZ < STOP_SCAN_MHZ):
                print("Max Start freq: " + str(objAnalazyer.MaxFreqMHZ)+"MHz")
                objRFE.Close()
                exit()
            else:
                if((StartFreqTemp + SpanSizeTemp) > STOP_SCAN_MHZ):
                    print("Max Stop freq (START_SCAN_MHZ + SPAN_SIZE_MHZ): " + str(STOP_SCAN_MHZ) +"MHz")
                    objRFE.Close()
                    exit()
                else:
                    StopFreqTemp = (StartFreqTemp + SpanSizeTemp)

    return SpanSizeTemp, StartFreqTemp, StopFreqTemp

class FreqWindow(Toplevel):

    def __init__(self, master = None):
        self.win = Toplevel

    def enterhandler(self, e):
        if DEBUG:
            print("FreqWindow enterhandler")

        app.freqwindow.get_entry()


    def draw_window(self):
        super().__init__(master = None)
        self.title("Frequency")
        self.geometry("600x200")
        self.columnconfigure(3, weight=2)
        freqlabel = Label(self, text ="Frequency (MHz): ", font=("Arial", 16));
        freqlabel.grid(row=0, column=0)
        
        self.freqentry = Entry(self, width = 15, font=("Arial", 16))
        self.freqentry.bind('<Return>', self.enterhandler)
        center_freq = int(((START_SCAN_MHZ) + (SPAN_SIZE_MHZ / 2)) ) 
        self.freqentry.insert(1, str(center_freq))
        self.freqentry.grid(row=0, column=1)


    def get_entry(self):
        global START_SCAN_MHZ, STOP_SCAN_MHZ, SPAN_SIZE_MHZ
       
        new_center = int(self.freqentry.get())

        START_SCAN_MHZ = new_center - (SPAN_SIZE_MHZ / 2)
        STOP_SCAN_MHZ = new_center + (SPAN_SIZE_MHZ / 2)

        app.init_window(content)

        self.win.destroy(self)

class SpanWindow(Toplevel):

    def __init__(self, master = None):
        self.win = Toplevel

    def enterhandler(self, e):
        if DEBUG:
            print("SpanWindow enterhandler")

        app.spanwindow.get_entry()

    
    def draw_window(self):
        super().__init__(master = None)
        self.title("Span")
        self.geometry("400x400")
        self.columnconfigure(3, weight=2)
        spanlabel = Label(self, text ="Span (Mhz): ", font=("Arial", 16));
        spanlabel.grid(row=0, column=0)
        
        self.spanentry = Entry(self, width = 15, font=("Arial", 16))
        self.spanentry.bind('<Return>', self.enterhandler)
        span = SPAN_SIZE_MHZ 
        self.spanentry.insert(1, str(span))
        self.spanentry.grid(row=0, column=1)

        current_state = "span"

    def get_entry(self):
        global START_SCAN_MHZ, STOP_SCAN_MHZ, SPAN_SIZE_MHZ
       
        new_span = int(self.spanentry.get())

        center = STOP_SCAN_MHZ - (SPAN_SIZE_MHZ / 2) 

        START_SCAN_MHZ = center - (new_span / 2)
        STOP_SCAN_MHZ = center + (new_span / 2)

        SPAN_SIZE_MHZ = new_span
        app.init_window(content)

        self.win.destroy(self)

class Window(Frame):
    def __init__(self, master = None):
        Frame.__init__(self, master)
        self.master = master
        self.freqwindow = FreqWindow(self.master)
        self.spanwindow = SpanWindow(self.master)

    def GetData(self, sweepdata, tot):
        amp = arr.array('f',[])
        freq = arr.array('f',[])
        for nDataPoint in range(tot):
            val = sweepdata.GetAmplitudeDBM(nDataPoint, None, False)
            amp.append(val)
            freq.append(sweepdata.GetFrequencyMHZ(nDataPoint))

        return freq, amp


    def animate(self,i):
        self.objSweep=None
        objRFE.ProcessReceivedString(True);
        num = objRFE.SweepData.Count 
        if self.numitems != num:
            self.numitems = num
            if (self.numitems > 0):
                objSweep=objRFE.SweepData.GetData(self.numitems-1)
                tot=objSweep.TotalDataPoints
                self.freq, self.amp = self.GetData(objSweep, tot)
                if DEBUG:
                    if i % 10 == 0:
                        print("in animate")
                        print("config start", objRFE.StartFrequencyMHZ, "config stop ", objRFE.StopFrequencyMHZ )
                        print("Freq min ", min(self.freq), " max ", max(self.freq))
                        print("Amp min ", min(self.amp), " max ", max(self.amp))

                 
                self.line.set_ydata(self.amp)  # update the data
                self.line.set_xdata(self.freq)  # update the data

                # display the peak values
                self.ann.remove()
                ymax = max(self.amp)
                xpos = np.argmax(self.amp)
                xmax = self.freq[xpos]

                xtextpos = 20
                ytextpos = 20

                greater_textsize = 200
                if xpos > SWEEPPOINTS - 10:
                    xtextpos = - greater_textsize
                
                
                xmax_int = int(xmax * 1000)
                xmax = xmax_int / 1000 
                
                display_text = "x = " + str((xmax)) + " y = " + str(ymax)
                self.ann = self.ax.annotate(display_text, xy=(xmax, ymax), textcoords='offset pixels', xytext=(xtextpos, ytextpos), 
#                self.ann = self.ax.annotate(display_text, xy=(xmax, ymax), xytext=(xmax + xtextpos, ymax + ytextpos), 
                        arrowprops=dict(arrowstyle="->", facecolor='black'),)
                objRFE.CleanSweepData()

        return self.line,


    def init_window(self, content):
        self.objSweep=None
        self.buttonFreq = Button(content,text="Frequency")
       
        self.freqwindow = FreqWindow(self.master)
        self.spanwindow = SpanWindow(self.master)

        # Following line will bind click event
        # On any click left / right button
        # of mouse a new window will be opened
        self.buttonFreq.bind("<Button>",
                 lambda e: self.freqwindow.draw_window())

        self.buttonFreq.grid(row=0,column=3, sticky= 'nsew')

        self.buttonAmp = Button(content,text="Span")
        self.buttonAmp.bind("<Button>",
                 lambda e: self.spanwindow.draw_window())
        self.buttonAmp.grid(row=1,column=3, sticky='nsew')

        self.fig = plt.Figure()
        self.ax = self.fig.add_subplot(111)
        self.canvas = FigureCanvasTkAgg(self.fig, master=frame)
        self.canvas.get_tk_widget().grid(column=0,row=0, sticky = 'nsew')

        self.build_plot(frame)


    def build_plot(self, frame):
        #Request RF Explorer configuration
        objRFE.SendCommand_RequestConfigData()

        #Wait to receive configuration and model details
        while(objRFE.ActiveModel == RFExplorer.RFE_Common.eModel.MODEL_NONE):
            objRFE.ProcessReceivedString(True)    #Process the received configuration


        #If object is an analyzer, we can scan for received sweeps
        if(not objRFE.IsAnalyzer()):
            if DEBUG:
                print("not an analyzer, exit", objRFE.IsAnalyzer() )
            objRFE.Close()
            exit()

        #Control settings
        SpanSize, StartFreq, StopFreq = ControlSettings(objRFE)

        if(SpanSize and StartFreq and StopFreq):
            nInd = 0
            #Set new configuration into device
            objRFE.UpdateDeviceConfig(StartFreq, StopFreq)
            objRFE.ProcessReceivedString(True)

            # wait until we have indication that the new configuration is in device
            while (StartFreq != objRFE.StartFrequencyMHZ):
                objRFE.ProcessReceivedString(True)

            self.numitems = objRFE.SweepData.Count - 1

            # wait until we have received data
            while (self.numitems <= 0):
                objRFE.ProcessReceivedString(True);
                self.numitems = objRFE.SweepData.Count - 1
            
            if DEBUG:
                print("config min", objRFE.StartFrequencyMHZ, "config max ", objRFE.StopFrequencyMHZ )

            if (self.numitems > 0):
                self.objSweep=objRFE.SweepData.GetData(self.numitems)
                tot=self.objSweep.TotalDataPoints
                self.freq, self.amp = self.GetData(self.objSweep, tot)
                if DEBUG:
                    print("Freq min ", min(self.freq), " max ", max(self.freq))
                    print("Amp min ", min(self.amp), " max ", max(self.amp))
                self.line, = self.ax.plot(self.freq, self.amp)        
                self.ax.axis([StartFreq, StopFreq, min(self.amp) - 10, 0 ])
                ymax = max(self.amp)
                xpos = np.argmax(self.amp)
                xmax = self.freq[xpos]
                display_text = "x = " + str(int(xmax)) + " y = " + str(ymax)
                self.ann = self.ax.annotate(display_text, xy=(xmax, ymax), xytext=(xmax+5, ymax + 5), arrowprops=dict(arrowstyle="->", facecolor='black'),)

            else: 
                if DEBUG:
                    print("somethings wrong")
        else:
            if DEBUG:
                print("Error: settings are wrong.\nPlease, change and try again")
        
        self.go_animate()

    def go_animate(self):
        self.ani = animation.FuncAnimation(self.fig, self.animate, None, interval=25, blit=False)


def enterhandler(e):
    global current_state

    if DEBUG:
        print("enterhandler", current_state, e)

    if current_state == "getport":
        go()
        current_state = ''

    if current_state == "freq":
        app.freqwindow.GetEntry()

    if current_state == "span":
        app.spanwindow.GetEntry()




def go():
    global app
    portno = int(user_entry[0].get())

    if portno >= len(objRFE.m_arrValidCP2102Ports) or portno < 0:
        if DEBUG:
            print("Error: Invalid Port Number", portno)
    else:
        PortName = objRFE.m_arrValidCP2102Ports[portno].device
        if DEBUG:
            print("Selected name", PortName)

    objRFE.ConnectPort(PortName, BAUDRATE)


    #Connect to available port
    if RESET:
        if DEBUG:
            print("Resetting device...")
        #Reset the unit to start fresh
        objRFE.SendCommand("r")
        #Wait for unit to notify reset completed
        while(objRFE.IsResetEvent):
            pass
        #Wait for unit to stabilize
        time.sleep(4)

    if DEBUG:
        print("Connected...")

    content, frame = build_window()
    app.init_window(content)  

def GetPort():
    global current_frame, current_state 

    current_frame.destroy()
    frame = Frame(root)
    current_frame = frame
    current_state = "getport"

    title = Label(current_frame, text= "Available Ports:", font=("Arial", 16))
    title.grid(row=0, column=1, padx=10, pady=10, sticky="ew")

    for objPort in objRFE.m_arrValidCP2102Ports:
        if (objPort.device == "/dev/ttyAMA0" or objPort.device == "/dev/ttyS4"):
            objRFE.m_arrValidCP2102Ports.remove(objPort)
 
    ctr = 0
    for objPort in objRFE.m_arrValidCP2102Ports:
        tmp_txt = str(ctr) + "     " +  objPort.device
        label = Label(current_frame, text=tmp_txt, font=("Arial", 16))
        # Entry widgets in the row whose index is idx
        label.grid(row=ctr+1, column=0, padx=10, pady=10, sticky="w")
        ctr += 1

    user_label = Label(current_frame, text= "Enter Port Number:", font=("Arial", 16))
    user_label.grid(row=ctr+2, column=0, padx=10, pady=10, sticky="w")
    user_entry.append(Entry(current_frame, width=5, font=("Ariel", 16)))
    user_entry[0].grid(row=ctr+2, column=1, padx=10, pady=10)
    user_entry[0].focus_set()

    current_frame.pack(fill="both", expand=True)

def build_window():
    global content, frame
    current_frame.destroy()

    root.geometry("800x500")
    content = ttk.Frame(root, padding=(3,3,12,12))
    frame = ttk.Frame(content, borderwidth=5, relief="ridge", width=200, height=100)

    content.grid(column=0, row=0, sticky='nsew')
    frame.grid(column=0, row=0, columnspan=3, rowspan=2, sticky='nsew')

    root.columnconfigure(0, weight=1)
    root.rowconfigure(0, weight=1)
    frame.columnconfigure(0, weight=1)
    frame.rowconfigure(0, weight=1)
    content.columnconfigure(0, weight=3)
    content.columnconfigure(1, weight=3)
    content.columnconfigure(2, weight=3)
    content.columnconfigure(3, weight=1)
    content.rowconfigure(0, weight=1)
    content.rowconfigure(1, weight=1)

   
    return content, frame

user_entry = []

objRFE = RFExplorer.RFECommunicator()

objRFE.GetConnectedPorts()

objRFE.VerboseLevel = 0



root = Tk(className='annName')
root.wm_title('RFE Signal Analyzer')
root.geometry('550x350+100+200')

root.title("sigann")
root.bind('<Return>', enterhandler)




current_frame = Frame(root)
app = Window(root)

GetPort()
tk.mainloop()
objRFE.Close()
