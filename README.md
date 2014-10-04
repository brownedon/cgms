cgms
====

Arduino to CGMS

I have stripped this down to the basics.  Works on a teensy 3.1, you'll need to make some mods to compile
  on a Mega ADK.  This should retrieve your current glucose every 5 minutes.  It also populates an array with
  your most recent values.  Limited attempt at power saving / sleep mode.  
  
I use the circuits@home mini usb, a teensy 3.1 and a sparkfun bluesmirf silver or gold.  
I use a battery pack and charger from adafruit.  
circuits@home provides libraries.

Good write up on the USB code at 
http://www.instructables.com/id/Control-an-Arduino-With-a-Wristwatch-TI-eZ430-Chr/step2/The-Arduino-Sketch/
I think I lifted the USB Portion from here, as it’s not in the libraries/examples for the circuits@home dongle.

Be careful with your microcontroller choice, you need allot of SRAM, the 2kb in the UNO isn’t enough to pull back the 1056 
byte pages of data.  Hence why I use the Teensy, the Mega ADK is also OK.

The mini usb dongle needs to be modified to work properly, it runs at 3v and the CGMS needs 5.  If you try this without making 
the modifications suggested by circuits@home, you’ll get a hardware battery fault from the CGMS.  
So, you need to modify the board, to isolate the USB power, and separately provide 5v.  Assuming you’re using a 3v lipo, 
you’ll need to step up the voltage, circuits@home has the board for that also.  

The mega adk will work as is, since it's 5v.  The full size circuits@home usb board is also 5v.

The code is a work in progress, I make no claims etc.  
You'll need to make changes depending on the micro you choose.


I should be able to get much more battery life by cutting the power to the CGMS, using a transistor as a switch off of one of 
the teensy pins.  
The issue with this is you need to reboot the microcontroller at wake up, otherwise the USB doesn't come up.  
I can get to 24 hours then.
Without shutting down the usb, you'll get 8-11 hours(1200 mAh battery).  When you are connected to the CGMS, it draws 100ma.  
The circuit I use can keep the CGMS at whatever charge level it was at when you plugged it in.
