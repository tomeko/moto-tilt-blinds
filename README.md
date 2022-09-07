
> :information_source: Note, this is an older/alternative implementation. The latest is in [the main branch](https://github.com/tomeko/moto-tilt-blinds) 


## Summary

Motorized tilt control for horizontal slat-style blinds over Wifi. There are quite a few similar projects & products out there but I couldn't find one that fit the requirements listed below:

<div>
  <div style="float: left;">
    <img src="https://user-images.githubusercontent.com/995615/146713471-8e8813d2-5823-4e75-b4cd-cd82f4d647de.gif" height="300px;" >
    <img src="https://user-images.githubusercontent.com/995615/152669667-17b6f8e3-7c39-4410-b6e5-10d9f201b2f4.gif" height="300px;" style="vertical-align: center;">
    <img src="doc/mobile-ui-ss0.png" height="300px;">
  </div>
</div>


### State of things
I've used this configuration for about 6 months on large (2m) blinds and it's been working well for the most part, though I think a redesign is in order for these reasons:
 - Had to replace the spool-shaft coupler twice because I overshot the closed position and it warped. That was a hassle.
 - I'm re-evaluating the benefit of keeping the existing pull string control versus driving the tilt-shaft directly for these reasons:
   - The accuracy of the controller mount holes is critical and they're not easy to drill out. For multiple installations this is a bit of work. 
   - The pull string control does backdrive the motor, so persons unaware could apply excessive force.
   - More parts to 3d print/assemble versus a direct-drive design
   - Since the replacement tilt-gear mechanism isn't needed, can use that cost for a better motor and driver (for large blinds)
  
In the meantime I recommend to consider this project: [Motorized_MQTT_Blinds](https://github.com/thehookup/Motorized_MQTT_Blinds) which offers a direct-drive design and is non-destructive.
In the next revision here I'll most likely go with direct-drive as well and have options per blinds length-size: Small (1m or less) and large (2m), and focus more on ease of installation.


### Features
  - **Retain pull-string configuration**: Retain manual tilt control of your blinds <sup>1</sup> 
  - **Keep the stock look**: No-show profile installation.
  - **Built in control via ESP8266 web interface:** Android OTA updatable firmware, SSDP device discovery.
  - **Low cost:** About $20-$50 per installation <sup>2</sup> 
  - **Optional manual control button**

<p><sup>1</sup>This backdrives the motor, so there's some extra resistance</p>
<p><sup>2</sup>As of Dec'21, depending on where you source the parts and how many you build.</p>


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
  
