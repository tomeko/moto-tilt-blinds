
## Summary

Motorized tilt control for horizontal slat-style blinds over Wifi. There are quite a few similar projects & products out there but I couldn't find one that fit the requirements listed below:

<div>
<img src="https://user-images.githubusercontent.com/995615/146713471-8e8813d2-5823-4e75-b4cd-cd82f4d647de.gif" height="260">
<img src="doc/mobile-ui-ss0.png" height="260">
</div>

### Features
  - **Back-driveable**: Retain manual tilt control of your blinds with existing pull strings in case of power/internet outage.
  - **Keep the stock look**: No clunky or visible parts after installation with no-show profile installation.
  - **Built in control via ESP8266 web interface:** Android OTA updatable firmware, SSDP device discovery.
  - **Low cost:** About $20-$50 per installation <sup>1</sup> 
  - **Optional manual control button**

<sup>1</sup>*As of Dec'21, depending on where you source the parts and how many you build.*

## Assembly/Installation

### Primary Requirements
  - Access to a 3d printer. (I used an Ender3 Pro, all printed with PLA)
  - Familiarity with soldering, wiring, etc.


*Note: This project requires drilling 3 holes into the metal blinds-housing to mount the controller/shaft-spool adapter, though this won't be visible when be covered-up by the housing shroud. Just something to consider if you're renting/etc*

[Installation guide](https://github.com/tomeko/moto-tilt-blinds/tree/main/doc)

## TODO/Wishlist
  - MQTT: If you're currently in that ecosystem, it would be nice. However this of course requires an external MQTT server, etc. Rev1 is good to go without any server requirements.
  - Multicast operation: Control multiple modules from one. Started implementing this but haven't finished. For now each installation has to be controlled from its own web interface.
  - Better module mounts: Some of the parts (e.g. D1-mini clone) don't have mounting holes, so Rev1 just uses thermal tape to insulate/consolidate parts. Even better a custom PCB for all this, but ain't no nobody got time for that...
  - Rev2: Use 433MHz transceivers with a central hub, battery power (rechargeable, low-power design will be tricky to allow extended use, e.g. > a few months without recharging). Not sure if I'll ever get around to this as the installed profile looks good for now. There are a some windows however that will not have a power outlet within a reasonable distance, so this would be nice.
  
