#!/bin/python3
#
#
# relay controller
#
#
#Note:  UDEV rule to set permissions:
#SUBSYSTEM=="usb", ATTRS{idVendor}=="20ce", ATTRS{idProduct}=="0022", \
# MODE="0666", GROUP="dialout"
#Place in /etc/udev/rules.d/43-mcl-relay-permissions.rules
#
#
#Jeff Porter
#6 June 2012
#


import usb.core
import usb.util
from usb.util import ENDPOINT_OUT, ENDPOINT_IN, endpoint_direction, find_descriptor, dispose_resources

class RelayError(Exception):
    pass
class RelayNotFoundError(Exception):
    pass
        

class RF_Relay(object):
    def __init__(self,switch_box,which_switch='A'):
        #print( "getting RF_Relay({},{})".format(switch_box,which_switch)
        self.switch_box = switch_box
        which_switch = which_switch.upper()

        model = self.switch_box.model ##getModel()
        self.model = model
        self.set_pos = self.set_pos_ss
        self.get_pos = self.get_pos_ss

        self.name = self.switch_box.name + which_switch
        #code below will need changes for dual SP6T switch box...
        if model=='USB-1SP8T-63H':
            self.cmd = ':SP8T:STATE'
        elif model=='USB-2SP4T-63H':
            self.cmd = ':SP4T:{}:STATE'.format(which_switch)
        elif model=='RC-2SPDT-A18':
            self.set_pos = self.set_pos_mech
            self.get_pos = self.get_pos_mech
            self.relay_num = "ABCDEF".index(which_switch)
        else:
            raise Exception("Unknown model type {}".format(model))

    def set_pos_mech(self,state):
        print( "setting {} to {}".format(self.name,state-1))
        self.switch_box.setRelayState(self.relay_num,state-1)
    def get_pos_mech(self):
        return self.switch_box.getRelayState(self.relay_num)+1

    def set_pos_ss(self,state):
        print( "setting {} to {}".format(self.name,state))
        self.switch_box.sendSCPI( self.cmd + ":{}".format(state))

    def get_pos_ss(self):
        res = int( self.switch_box.sendSCPI( self.cmd + "?" ) )
        return res
        
class SwitchBox(object):
    def __init__(self,serial_in="*",model_in="*",open_relay=True):
        self.relay = MCRelay(serial_in,model_in,open_relay)
    def get_pos(self):
        return self.relay.getAllRelays()
    def set_pos(self,state):
        self.relay.setAllRelays(state)
    def getModel(self):
        return self.relay.getModel()

class RelayClass(object):
    def __init__(self,address=None):
        """ open a connection to the relay device at the given address """
        pass
    def getNumberRelays(self):
        """ Return number of relays """
        pass
    def setRelayState(self,relay,state):
        """ set state of given relay (0..N-1), 0/false or 1/true """
        pass
    def getRelayState(self,relay,state):
        """ return state of given realy (0..N-1), 0=not engaged, 1=engaged """
        pass
    def setAllRelays(self,state):
        """ set all relays to mask provided """
        for k in range(0,self.getNumberRelays()):
            self.setRelayState(k,state & (1<<k))
    def getAllRelays(self):
        """ return mask of all relay values """
        res = 0
        for k in range(0,self.getNumberRelays()):
            if self.getRelayState(k):
                res |= (1<<k)
        return res




class MCRelay(RelayClass):
    VENDOR_ID = 0x20ce
    DEVICE_ID = 0x0022

    def __init__(self,serial_in="*",model_in="*",open_relay=True,name=None):
        self.name = name
        print( "opening ",name," serial=",serial_in)
        if open_relay:
            self._open_relay(serial_in,model_in)
        else:
            self.ep_out = None
            self.ep_in = None

    def _is_match(self,A,B):
        #A and B are tuples of (SerialNumber,Model)
        #B is the pattern to match against, where "*" is a wildcard
        #turns true if A[0] ends with B[0] (serial number partial match from end and if
        #model number A[1] contains B[1].
        #None is considered a wild card
        return (B[0]=="*" or A[0].endswith(B[0])) and (B[1]=="*" or A[1].find(B[1])>-1)


    def _get_all_relays(self,serial_in="*",model_in="*"):
        """ returns an iterator of (serial,model,(device,ep_out,ep_in)) that match the serial number and/or model.

            Note that the device resources are released (disposed) once the iterator moves to the next device.
        """
      
        for dev in usb.core.find(idVendor=MCRelay.VENDOR_ID, idProduct=MCRelay.DEVICE_ID, find_all=True):

            try:
                dev.detach_kernel_driver(0)
            except:
                pass

            try:
                #the reset call slowed things down and also caused problems when opening multiple devices.
                #dev.reset()
                dev.set_configuration()
            except usb.core.USBError:
                dispose_resources(dev)
                continue  # ignore error, skip to next device (current device might already be open)
            
            cfg = dev.get_active_configuration()
            interface_number = cfg[(0,0)].bInterfaceNumber
            intf = usb.util.find_descriptor(
                cfg, bInterfaceNumber = interface_number)

            ep_out = find_descriptor( intf,
                custom_match = lambda e: endpoint_direction(e.bEndpointAddress) == ENDPOINT_OUT )

            ep_in = find_descriptor(intf,
                custom_match = lambda e: endpoint_direction(e.bEndpointAddress)== ENDPOINT_IN )

            assert ep_out is not None
            assert ep_in is not None

            sn = self.getSN((ep_out,ep_in))
            model = self.getModel((ep_out,ep_in))

            if self._is_match( (sn,model), (serial_in,model_in) ):
                yield (sn,model,(dev,ep_out,ep_in))
            
            dispose_resources(dev)

        raise StopIteration

    def get_relay_list(self,serial_in="*",model_in="*"):
        for sn,model,dev in self._get_all_relays(serial_in,model_in):
            yield (sn,model)

    def _open_relay(self,serial_in="*",model_in="*"):

        try:
            sn,model,device_info = next(self._get_all_relays(serial_in,model_in))
        except StopIteration:
            raise RelayNotFoundError

        self.serial=sn
        self.model=model
        self.dev = device_info[0]
        self.ep_out = device_info[1]
        self.ep_in = device_info[2]

        try:
            #4th character of model is # of switches (?)
            self.numRelays = int(self.model[3])
        except ValueError:
            self.numRelays = 0 # can't figure it out

    def getNumberRelays(self):
        return self.numRelays

    def write(self,msg,end_points=None):
        if len(msg)>64:
            raise RelayError("Message is too long")
        msg = bytearray(msg) + bytearray(64-len(msg))

        if end_points==None:
            end_points = (self.ep_out,self.ep_in)

        end_points[0].write(msg, 1000)
        res = end_points[1].read(64,1000)
        return bytearray(res)

    def getSN(self,end_points=None):

        sn = self.write(bytearray([41]),end_points)
        end_pos = bytes(sn).find(b"\0")
        if end_pos==-1:
            print( "failed to read S/N correctly")
        sn = sn[1:end_pos].decode('utf-8')
        return sn


    def sendSCPI(self,cmd):
        res = self.write(bytearray([1])+bytearray(cmd,'utf-8'))
        end_pos = bytes(res).find(b"\0")
        return res[1:end_pos].decode('utf-8')

    def getModel(self,end_points=None):
        model = self.write(bytearray([40]),end_points)
        end_pos = bytes(model).find(b"\0")
        if end_pos==-1:
            print( "failed to read model correctly")
        model = model[1:end_pos].decode('utf-8')
        return model

    def setRelayState(self,relay,state):
        if state:
            val = 1
        else:
            val = 0
        self.write( bytearray([relay+1,val]) )
        self.getSN() # dummy read

    def setAllRelays(self,state):
        self.write( bytearray([9,state]) )
        self.getSN() # dummy read

    def getAllRelays(self):
        msg = self.write( bytearray([15]) )
        self.getSN() # dummy read
        return int(msg[1])

    def getRelayState(self,relay):
        state = self.getAllRelays()
        self.getSN() # dummy read

        if state & (1<<relay):
            return 1
        else:
            return 0

    def tryit(self,msg):
        res = self.write( bytearray([msg]) )
        return bytearray(res[0:10])

def Relay(addr,**kwargs):

    if addr.lower()=='mc' or addr.lower()=='minicircuits':
        return MCRelay(**kwargs)
    else:

        raise RelayError("Cannot determine relay to open")



if __name__=='__main__':
    import argparse

    parser = argparse.ArgumentParser(description="RF Relay Controller (MCL USB Relays)")

    parser.add_argument("--info", default=False, action="store_true",
                       help="Print relay serial number")

    parser.add_argument("state", nargs="?", default=None, type=int,
                        help="Relay settings (bit mask)")

    parser.add_argument("--sn", "--serial", "--SN", default="*",
                        help="Device serial number")
    
    parser.add_argument("--model", default="*",
                        help="Device model")
    
    parser.add_argument("--list", default=False, action="store_true",
                        help="Print list of USB relays found")

    parser.add_argument("--read", default=False, action="store_true",
                        help="Read state of given relay unit")
    
    


    args = parser.parse_args()

    if args.list:
        r = Relay('mc', open_relay=False)
        for sn,model in r.get_relay_list(args.sn, args.model):
            print( "Model %s SN %s" % (model,sn))


    else:
        r = Relay('mc',serial_in=args.sn,model_in=args.model)
        if args.info:
            print( "Model = %s, SN = %s" % (r.getModel(), r.getSN()))
        if args.read:
            print( "State = %02x" % (r.getAllRelays()))

        if args.state!=None:
            r.setAllRelays( args.state )

        
