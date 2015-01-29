
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
