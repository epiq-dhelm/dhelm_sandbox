import sys
import time
import serial.tools.list_ports
import serial
import platform
import threading

class USBConnect(object):    
    """Main API class to support all basic low level operations with RF Explorer
	"""
    def __init__(self):
        self.m_bAutoConfigure = True 
        self.m_arrConnectedPorts = []
        self.m_arrValidCP2102Ports = []
        self.m_hSerialPortLock = threading.Lock()
        self.m_nVerboseLevel = 1
        self.m_bIsResetEvent = False
        self.m_objSerialPort = serial.Serial()
        self.m_hQueueLock = threading.Lock()
        self.m_ReceivedBytesMutex = threading.Lock()

    @property
    def VerboseLevel(self):
        """Debug trace level for print console messages 1-10 being 10 the most verbose
	    """
        return self.m_nVerboseLevel
    @VerboseLevel.setter
    def VerboseLevel(self, value):
        self.m_nVerboseLevel = value
    @property
    def AutoConfigure(self): 
        """Auto configure is true by default and is used for the communicator to auto request config data to RFE upon port connection
		"""
        return self.m_bAutoConfigure
    @AutoConfigure.setter
    def AutoConfigure(self, value): 
        self.m_bAutoConfigure = value

    def SendCommand(self, sCommand):
        """Format and send command - for instance to reboot just use "r", the '#' decorator and byte length char will be included within
         
        Parameters: www.rf-explorer.com/API 
            sCommand -- Unformatted command from http://www.rf-explorer.com/API
		"""
        sCompleteCommand = "#" + chr(len(sCommand) + 2) + sCommand
        self.m_objSerialPort.write(sCompleteCommand.encode('utf-8'))
        if self.m_nVerboseLevel>5:
            print("RFE Command: #(" + str(len(sCompleteCommand)) + ")" + sCommand + " [" + " ".join("{:02X}".format(ord(c)) for c in sCompleteCommand) + "]")

    def SendCommand_RequestConfigData(self):
        """Request RF Explorer SA device to send configuration data and start sending feed back
		"""
        self.SendCommand("C0")

    def SendCommand_GeneratorRFPowerOFF(self):
        """Set RF Explorer RFGen device RF power output to OFF
		"""
        self.SendCommand("CP0")

    def IsConnectedPort(self, sPortName):
        """True if it is possible connect to specific port, otherwise False 
            
        Parameters:
            sPortName -- Serial port name, can take any form accepted by OS
        Returns:
            Boolean True if it is possible connect to specific port, otherwise False
		"""
        self.m_objSerialPort.baudrate = 500000
        self.m_objSerialPort.port = sPortName
        bOpen = False
        try:
            self.m_hSerialPortLock.acquire()
            self.m_objSerialPort.open()
        except Exception as obEx:
            print("Error in RFCommunicator - IsConnectedPort()" + str(obEx))
        finally:
            if(self.m_objSerialPort.is_open):
                bOpen = True
            self.m_objSerialPort.close()
            self.m_hSerialPortLock.release()

        return bOpen

    def GetConnectedPorts(self):
        """ Found the valid available serial port

        Returns:
            Boolean True if it found valid available serial port, False otherwise
		"""
        bOk = True
        sValidPorts = ""
        self.m_arrValidCP2102Ports = []

        try:
            self.m_arrConnectedPorts = list(serial.tools.list_ports.comports())
            if self.m_nVerboseLevel>5:
                print(str(len(self.m_arrConnectedPorts)))
            if(self.m_arrConnectedPorts):
                if self.m_nVerboseLevel>5:
                    print("Detected COM ports:")
                    for objPort in self.m_arrConnectedPorts:
                        print("  * " + objPort.device)

                sSystem = platform.system()

                if self.m_nVerboseLevel>5:
                    print("Detected OS: " + sSystem)

                for objPort in self.m_arrConnectedPorts:
                    if(self.IsConnectedPort(objPort.device)):
                        if(sSystem == "Darwin"):
                            #MAC OS. In macOS we limit valid ports to those using the SILABS driver
                           if ("SLAB_USB" in objPort.device):
                                self.m_arrValidCP2102Ports.append(objPort)
                                if self.m_nVerboseLevel>5:
                                    print(objPort.device + " is a valid available port.")
                                sValidPorts += objPort.device + " "
                        else:
                            #Windows, Linux, etc. Autodectect function is not working with virtual serial port.
                            self.m_arrValidCP2102Ports.append(objPort)
                            if self.m_nVerboseLevel>5:
                                print(objPort.device + " is a valid available port.")
                            sValidPorts += objPort.device + " "
                for objPort in self.m_arrValidCP2102Ports:
                    if (objPort.device == "/dev/ttyAMA0" or objPort.device == "/dev/ttyS4"):
                        self.m_arrValidCP2102Ports.remove(objPort)
                        tmpstr = sValidPorts.replace(objPort.device, '')
                        sValidPorts = tmpstr

                if(len(self.m_arrValidCP2102Ports) > 0):
                    if self.m_nVerboseLevel>5:
                        print("RF Explorer Valid Ports found: " + str(len(self.m_arrValidCP2102Ports)) + " - " + sValidPorts)
                else:   
                    print("Error: not found valid COM Ports") 
                    bOk = False
            else: 
                print("Serial ports not detected")
                bOk = False
        except Exception as obEx:
            print("Error scanning COM ports: " + str(obEx))
            bOk = False   
        
        return bOk

    def IsConnectedPort(self, sPortName):
        """True if it is possible connect to specific port, otherwise False 
            
        Parameters:
            sPortName -- Serial port name, can take any form accepted by OS
        Returns:
            Boolean True if it is possible connect to specific port, otherwise False
		"""
        self.m_objSerialPort.baudrate = 500000
        self.m_objSerialPort.port = sPortName
        bOpen = False
        try:
            self.m_hSerialPortLock.acquire()
            self.m_objSerialPort.open()
        except Exception as obEx:
            print("Error in RFCommunicator - IsConnectedPort()" + str(obEx))
        finally:
            if(self.m_objSerialPort.is_open):
                bOpen = True
            self.m_objSerialPort.close()
            self.m_hSerialPortLock.release()

        return bOpen

    def ConnectPort(self, sUserPort, nBaudRate):
        """Connect serial port and start init sequence if AutoConfigure property is set. Connect automatically
        if sUserPort is None and there is only one available serial port, otherwise show an error message

        Parameters:
            sUserPort -- Serial port name, can take any form accepted by OS
            nBaudRate -- Usually 500000 or 2400, can be -1 to not define it and take default setting
        Returns:
		    Boolean True if port is open, otherwise False
		"""
        bConnected = False
        if(self.m_nVerboseLevel > 0):
            sErrorText = "User COM port: "
            if(sUserPort):
                sErrorText += sUserPort
            else:
                sErrorText += "void"
            print(sErrorText)
            
        try:
            self.m_hSerialPortLock.acquire()
            #if the user defined a port then see if it is configured
            if(sUserPort):                              
                for sPort in self.m_arrValidCP2102Ports:
                    if(sUserPort == sPort.device):
                        sPortName = sUserPort
                        bConnected = True
            #if automatic then see if there is only one port
            elif (len(self.m_arrValidCP2102Ports) == 1):
                sPortName = self.m_arrValidCP2102Ports[0].device
                bConnected = True

            #if there are more than one port, remove ignored ports
            elif(len(self.m_arrValidCP2102Ports) >= 2):
                for objPort in self.m_arrValidCP2102Ports:
                    if (objPort.device == "/dev/ttyAMA0" or objPort.device == "/dev/ttyS4"):
                        self.m_arrValidCP2102Ports.remove(objPort)
                #if there is only one port left use it.
                if(len(self.m_arrValidCP2102Ports) == 1):
                    sPortName = self.m_arrValidCP2102Ports[0].device
                    bConnected = True
                    if self.m_nVerboseLevel>5:
                        print("Automatically selected port" + sPortName +" - ttyAMA0 and ttys4 ignored")
                    """
                else:
                    #there are still more than one valid port ask the user
                    print("Available Ports:")
                    while (bConnected == False):
                        i = 0
                        for objPort in self.m_arrValidCP2102Ports:
                            print(i, objPort.device)
                            i += 1
                        print("Enter port number")
                        inpt = input()
                        p = int(inpt)
                        if p >= len(self.m_arrValidCP2102Ports) or p < 0:
                            print("Invalid input", p)
                        else:
                            sPortName = self.m_arrValidCP2102Ports[p].device
                            print("Selected name", sPortName)
                            bConnected = True

        """ 
            if(bConnected):
                self.m_objSerialPort.baudrate = nBaudRate
                self.m_objSerialPort.port = sPortName
                self.m_objSerialPort.bytesize = serial.EIGHTBITS   
                self.m_objSerialPort.stopbits= serial.STOPBITS_ONE 
                self.m_objSerialPort.Parity = serial.PARITY_NONE   
                self.m_objSerialPort.timeout = 100  

                self.m_objSerialPort.open()

                self.m_bPortConnected = True

                if self.m_nVerboseLevel>5:
                    print("Connected: " + str(self.m_objSerialPort.port) + ", " + str(self.m_objSerialPort.baudrate) + " bauds")

                time.sleep(0.5)
                if (self.m_bAutoConfigure):
                    self.SendCommand_RequestConfigData()
                    time.sleep(0.5)
            else:             
                print("Error: select a different COM port")

        except Exception as obEx:
            print("ERROR ConnectPort: " + str(obEx))
        finally:
            self.m_hSerialPortLock.release()
        return self.m_objSerialPort.is_open   
        
    def ClosePort(self):
        """ Close port and initialize some settings

        Returns:
            Boolean True if serial port was closed, False otherwise
		"""
        try:
            self.m_hSerialPortLock.acquire()

            if (self.m_objSerialPort.is_open):
                time.sleep(0.2)
                if (self.IsAnalyzer()):
                    if (self.m_eMode == RFE_Common.eMode.MODE_SNIFFER):
						#Force device to configure in Analyzer mode if disconnected - C0 will be ignored so we send full config again
                        self.SendCommand("C2-F:" + str(self.StartFrequencyMHZ * 1000.0) + "," + str(self.CalculateEndFrequencyMHZ() * 1000) + "," +
                                         str(self.AmplitudeTopDBM) + "," + str(self.AmplitudeBottomDBM))
                    if (self.m_eMode != RFE_Common.eMode.MODE_SPECTRUM_ANALYZER and self.m_eMode != RFE_Common.eMode.MODE_SNIFFER):
						#If current mode is not analyzer, send C0 to force it
                        self.SendCommand_RequestConfigData()

                    time.sleep(0.2)
                    self.SendCommand_Hold() #Switch data dump to off
                    time.sleep(0.2)
                    if (self.m_objSerialPort.baudrate < 115200):
                        time.sleep(0.2)
                else:
                    self.SendCommand_GeneratorRFPowerOFF()
                    time.sleep(0.2)
                time.sleep(0.2)
                self.SendCommand_ScreenON()
                self.SendCommand_DisableScreenDump()

                #Close the port  
                if self.m_nVerboseLevel>5:
                    print("Disconnected.")
                self.m_objSerialPort.close()

                self.m_bPortConnected = False #do this here so the external event has the right port status

        except Exception:
            pass
        finally: 
            self.m_hSerialPortLock.release()

        self.m_bPortConnected = False  #to be double safe in case of exception

        return (not self.m_objSerialPort.is_open)

