
# Direct to Rift notes

## Setup 

Configure rendering with ovrHmd_ConfigureRendering() 

May need HWND window and HDC device context — not clear whether optional. 

Windows users will need to call ovrHmd_AttachToWindow() to direct its swap-chain output to the HMD. Needs HWND window.

Configure textures in ovrGLTexture (just needs textureID)

## Rendering

See p33 of guide.

All on render thread. Can either use one texture per eye, or same texture with half viewport.

ovrFrameTiming <- ovrHmd_BeginFrame()
ovrHmd_GetHmdPosePerEye()
<draw 1st eye world into render texture>
ovrHmd_GetHmdPosePerEye()
<draw 2nd eye world into render texture>
ovrHmd_EndFrame() // distortion render, buffer swap, gpu sync

In Max this can be triggered in the [t b b erase] kind of setup perhaps. Need to find out what hooks can be made into jit.gl.node in case that would make it better? 

## TODO:

ovr_Shutdown(). It was crashy before when I did it in object release. Is there a way to signal when an external is unloaded?

----
https://github.com/federico-mammano/Oculus-SDK-0.4.4-beta-OpenGL-Demo -- opengl version of tinyroom

----
http://renderingpipeline.com/2014/07/opengl-on-oculus-rift-dk2/

The first pitfall is the order of OculusSDK initialisation, HMD creation, window and OpenGL context generation. The best working order seems to be the following:

ovr_Initialize
ovrHmd_Create
glfwCreateWindow – will create a window and a OpenGL context
ovrHmd_AttachToWindow
ovrHmd_ConfigureRendering
Note that the window & GL context (step 3) should not be done before the Oculus SDK initialisations and that ovrHmd_AttachToWindow only works for me if I perform this before the ovrHmd_ConfigureRendering call, even tho the order described in the SDK documentation is the other way around (see page 29).