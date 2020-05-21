# MpxHid
Control the Mpxplay Media player (even under DOS) with just a Pro Micro, Rotary Encoder and a little Nokia 5110 display (no keyboard required)...

Hackaday: https://hackaday.io/project/171821-control-dos-media-player-with-pro-micro

Basically you have a list of menu options (each of which can have sub menus) from which you select the required actions. Simply navigate to the option you want and press the button to generate the appropriate keyboard action. In theory you can generate hundreds of actions form a handful of menus.

Options:
In place of the 5110 you could use a Touch Screen Displays where various graphical representations of buttons or images/icons once pressed would cause an action just like a macro keyboard. Each screen could represent a different application or more actions, it's entirely up to you...

In the original design key actions are hardcoded saved at compile time but it would be possible to incorporate an SD card and perhaps using an existing application code a file and save macros to the SD card. On boot by the device the card could be read to transfer the macros to the device...

MicroProcessor: Pro Micro
IDE: Arduino

Mike
