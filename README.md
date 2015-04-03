oculus
======

A Max/MSP/Jitter object to provide access to the Oculus Rift HMD via the LibOVR SDK.

Work in progress; [see discussion on the C74 forum](http://www.cycling74.com/forums/topic/oculus-rift).

Supports DK2, built against LibOVR 0.4.4; requires the corresponding drivers from [Oculus](https://developer.oculus.com/) to be installed. Should also support DK1, but not recently tested.

Supports OSX and Windows (32-bit only)

Implemented:

- Tracking
	- Orientation tracking (as Jitter-compatible quat)
	- Position tracking & status
	- Example integrating HMD tracking with in-world navigation

- Rendering
	- Output rift distortion parameters, including distortion mesh
	- Example distortion shaders, including chromatic aberration correction
	- Example rendered scene

- Display management
	- If >1 displays, additional monitor view opens of first display when HMD goes fullscreen, and Rift view opens on Nth display.
	- Automatically orients window according to display orientation (landscape/portrait)

Known issues:

- Direct-to-Rift is not currently supported, because the  LibOVR SDK is not currently compatible with OpenGL in a non-dedicated application. 
	
TODO:

- Optimized predictive tracking
- Timewarp rendering
- Health & safety warning overlay
