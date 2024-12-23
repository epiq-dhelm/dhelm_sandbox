import usb.core
import usb.util

#find our device
dev = usb.core.find(idVendor=0x20ce, idProduct=0x0022)

if dev is None:
   raise ValueError('Device not found')

for configuration in dev:
   for interface in configuration:
      ifnum = interface.bInterfaceNumber
      if not dev.is_kernel_driver_active(ifnum):
         continue
      try:
         dev.detach_kernel_driver(ifnum)
      except usb.core.USBError, e:
         pass

#set the active configuration. with no args we use first config.
dev.set_configuration()
SerialN=""
ModelN=""
Fw=""

dev.write(1,"*:SN?") 
sn=dev.read(0x81,64) 
i=1
while (sn[i]<255 and sn[i]>0):  
   SerialN=SerialN+chr(sn[i])
   i=i+1
   
dev.write(1,"*:MN?") 
mn=dev.read(0x81,64) 
i=1
while (mn[i]<255 and mn[i]>0):  
   ModelN=ModelN+chr(mn[i])
   i=i+1
   
dev.write(1,"*:FIRMWARE?") 
sn=dev.read(0x81,64) 
i=1
while (sn[i]<255 and sn[i]>0):  
   Fw=Fw+chr(sn[i])
   i=i+1

print (ModelN)
print (SerialN)
print (Fw)

# Set switch state (SP16T)
dev.write(1,"*:SP16T:STATE:16;")
resp=dev.read(0x81,64)
i=1
AttResp=""
while (resp[i]<255 and resp[i]>0):  
   AttResp=AttResp+chr(resp[i])
   i=i+1 
print AttResp

# Read switch state (SP16T)
dev.write(1,"*:SP16T:STATE?")
resp=dev.read(0x81,64)
i=1
AttResp=""
while (resp[i]<255 and resp[i]>0):  
   AttResp=AttResp+chr(resp[i])
   i=i+1 
print AttResp
