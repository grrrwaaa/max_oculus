oculus
======

A Max/MSP/Jitter object to provide access to the Oculus Rift HMD via the LibOVR SDK. 

For development history & feedback, also [see discussion on the C74 forum](http://www.cycling74.com/forums/topic/oculus-rift).

Here are some projects that have used this external:

- [Whiter Room](https://cycling74.com/project/whiter-room-oculus-video/), Tobias Rosenberger
- [Dance in the Rain](http://cargocollective.com/YKBX/Koda-Kumi-Dance-In-The-Rain-VR-Music-Video), Masato Tsutsui

## Requirements

Supports Max 7 on OSX and Windows (use the 32-bit Max 7 version on Windows). Might also work in Max 6.10, but support is not guaranteed.

Currently supports Oculus Developer Kit 2. Probably also works with Developer Kit 1, but not currently tested. Currently built against the LibOVR 0.4.4 beta.

Oculus development in general requires a powerful GPU; integrated cards are unlikely to be sufficient.

*Note that an Oculus HMD is not required to be present to work with the external; an offline simulator will be used when an HMD is not attached.*

## Installation & setup

- Install the LibOVR 0.4.4 runtime/driver from [Oculus](https://developer.oculus.com/downloads/#version=pc-0.4.4-beta). Later versions of the runtime *might* work. 

- Once installed, run the configuration utility to configure the device for your system. 

- Make sure that the runtime is set to 'extended mode'. Direct-to-HMD mode cannot not currently be used (see notes below).

- Configure display refresh rates to prevent judder (see notes below).

- Download the repository directory and unzip into ```Documents/Max 7/Packages``` (OSX) or ```My Documents/Max 7/Packages``` (Windows).

- Restart Max, and open the ```help/oculus.maxhelp``` to check that it is functioning correctly.

## Supported features

- Tracking
	- Orientation tracking (as Jitter-compatible quat)
	- Position tracking & status
	- Example integrating HMD tracking with in-world navigation (WASD + arrow keys)

- Rendering
	- Output rift distortion parameters, including distortion mesh
	- Example distortion shaders, including chromatic aberration correction
	- Example rendered scene
	- FPS monitoring (press 'f' to show in-world)
	- Low persistence rendering option (press 'p' to toggle this)

- Display management
	- If more than one display is present, an additional monitor view opens on the first display when HMD goes fullscreen, and the Rift view full-screens on nth display.
	- Automatically orients window according to display orientation (landscape/portrait)
	
## TODO

- Optimized predictive tracking.
- Health & safety warning overlay.
- Timewarp rendering. 
- Direct-to-Rift / SDK-rendering. Not currently possible, due to the design of the LibOVR SDK, which shims the OpenGL graphics driver, and requires this to occur before any other OpenGL calls. It is therefore incompatible with a plugin system such as Max/MSP/Jitter externals.

## Notes on jitter & judder

Getting a stable experience with the oculus can present some difficulties. 

**Display refresh rate**: If you have another display attached to the computer (as is likely), it is very important that both the Oculus Rift and the main display are configured to the same refresh rate. If this is not the case you are very likely to experience a nauseating judder when turning your head in the Rift display. Use the display manager (Windows display manager, or Nvidia/AMD equivalent, or OSX system preferences) to reconfigure the refresh rate. (Perhaps jit.displays might also be able to do this?) The Oculus Rift performs better at 75Hz; you may have to reduce the resolution on your main display in order to enable a 75Hz refresh rate there. If this is not possible, set the Rift to 60Hz (but this will result in a more noticeable motion blur or flicker). If the display manager does not allow you to change the refresh rate (e.g. some OSX retina displays), you may have to disconnect the display entirely.

**Patcher frame rate**: Additionally, the patcher must be able to render at 60-75fps to prevent nausea. If your GPU is struggling to keep this up, you can try reducing the render texture size (see help patcher). 

## License

MIT license; copyright 2014-2015 Graham Wakefield.