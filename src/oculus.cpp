/**
	@file
	oculus - a max object
 */

#ifdef __cplusplus
extern "C" {
#endif
#include "ext.h"
#include "ext_obex.h"
    
#include "z_dsp.h"
    
#include "jit.common.h"
#include "jit.gl.h"
#ifdef __cplusplus
}
#endif

#include <new>

#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"

#define OCULUS_DEFAULT_TEXTURE_DIM 2048

t_class *oculus_class;
static t_symbol * ps_quat;
static t_symbol * ps_pos;
static t_symbol * ps_warning;

volatile int libcount = 0;

class t_oculus {
public:
    t_object	ob;			// the object itself (must be first)
    
    int			fullview;
    double      predict;    // time (in ms) to predict ahead tracking
    
    // many outlets:
    void *		outlet_q;
    void *		outlet_p;
    void *		outlet_p_info;
    void *		outlet_eye[2];
    void *		outlet_mesh[2];
    void *		outlet_c;
    
    // the quaternion orientation of the HMD:
    t_atom		quat[4];
    t_atom		pos[3];

	// the dimension of the texture passed to direct render
	ovrSizei	texture_dim;
	int			multisample;
    
    // LibOVR objects:
    ovrHmd		hmd;
    
    t_oculus(int index = 0) {
        t_atom a[4];
        
        outlet_c = outlet_new(&ob, 0);
        outlet_mesh[1] = outlet_new(&ob, "jit_matrix");
        outlet_eye[1] = outlet_new(&ob, 0);
        outlet_mesh[0] = outlet_new(&ob, "jit_matrix");
        outlet_eye[0] = outlet_new(&ob, 0);
        outlet_p_info = outlet_new(&ob, 0);
        outlet_p = listout(&ob);
        outlet_q = listout(&ob);
        
        atom_setfloat(quat+0, 0);
        atom_setfloat(quat+1, 0);
        atom_setfloat(quat+2, 0);
        atom_setfloat(quat+3, 1);
        
        atom_setfloat(pos+0, 0.f);
        atom_setfloat(pos+1, 0.f);
        atom_setfloat(pos+2, 0.f);
        
        fullview = 1;
		multisample = 1;

		texture_dim.w = 2048;
		texture_dim.h = 2048;
        
        libcount++;
        if (libcount == 1) {
            if (!ovr_Initialize()) {
                object_error(&ob, "LibOVR: failed to initialize library");
                return;
            } else {
                object_post(&ob, "initialized LibOVR %s", ovr_GetVersionString());
            }
        }
        
        int hmd_count = ovrHmd_Detect();
        object_post(&ob, "%d HMDs detected", hmd_count);
        if (hmd_count > 0 && index < hmd_count) {
            hmd = ovrHmd_Create(index);
        }
        hmd = ovrHmd_Create(index);
        if (hmd == 0) {
            object_warn(&ob, "LibOVR: HMD not detected, using offline DK2 simulator instead");
            hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
        }
        
        // configure tracking:
        // in this case, try to use all available tracking capabilites,
        // but do not require any to be present for success:
        if (!ovrHmd_ConfigureTracking(hmd, hmd->TrackingCaps, 0)) {
            object_error(&ob, "failed to configure/initiate tracking");
        }
        
        
    }
    
    ~t_oculus() {
        
        if (hmd) {
            //			ovrHmd_StopSensor(hmd);
            ovrHmd_Destroy(hmd);
        }
        
        // release library nicely
        //libcount--; // was crashy
        if (libcount == 0) {
            ovr_Shutdown();
        }
    }
    
    void configure() {
        t_atom a[4];
        
        if (!hmd) {
            object_warn(&ob, "No HMD to configure");
            return;
        }
        
        // note serial:
        atom_setsym(a, gensym(hmd->SerialNumber));
        outlet_anything(outlet_c, gensym("serial"), 1, a);
        
        
    #define HMD_CASE(T) case T: { \
            atom_setsym(a, gensym( #T )); \
            outlet_anything(outlet_c, gensym("hmdType"), 1, a); \
            break; \
        }
        switch(hmd->Type) {
                HMD_CASE(ovrHmd_DK1)
                HMD_CASE(ovrHmd_DKHD)
                HMD_CASE(ovrHmd_DK2)
            default: {
                atom_setsym(a, gensym("unknown"));
                outlet_anything(outlet_c, gensym("Type"), 1, a);
            }
        }
    #undef HMD_CASE
        
        atom_setsym(a, gensym(hmd->Manufacturer));
        outlet_anything(outlet_c, gensym("Manufacturer"), 1, a);
        atom_setsym(a, gensym(hmd->ProductName));
        outlet_anything(outlet_c, gensym("ProductName"), 1, a);
        
        atom_setlong(a, hmd->FirmwareMajor);
        atom_setlong(a+1, hmd->FirmwareMinor);
        outlet_anything(outlet_c, gensym("Firmware"), 2, a);
        
        atom_setfloat(a, hmd->CameraFrustumHFovInRadians);
        outlet_anything(outlet_c, gensym("CameraFrustumHFovInRadians"), 1, a);
        atom_setfloat(a, hmd->CameraFrustumVFovInRadians);
        outlet_anything(outlet_c, gensym("CameraFrustumVFovInRadians"), 1, a);
        atom_setfloat(a, hmd->CameraFrustumNearZInMeters);
        outlet_anything(outlet_c, gensym("CameraFrustumNearZInMeters"), 1, a);
        atom_setfloat(a, hmd->CameraFrustumFarZInMeters);
        outlet_anything(outlet_c, gensym("CameraFrustumFarZInMeters"), 1, a);
        
        atom_setsym(a, gensym(hmd->DisplayDeviceName));
        outlet_anything(outlet_c, gensym("DisplayDeviceName"), 1, a);
        atom_setlong(a, hmd->DisplayId);
        outlet_anything(outlet_c, gensym("DisplayId"), 1, a);
        atom_setlong(a, hmd->Resolution.w);
        atom_setlong(a+1, hmd->Resolution.h);
        outlet_anything(outlet_c, gensym("Resolution"), 2, a);
        
        atom_setlong(a, hmd->EyeRenderOrder[0]);
        outlet_anything(outlet_c, gensym("EyeRenderOrder"), 1, a);
        
        
        // capabilities:
        //hmd->HmdCaps
        //hmd->SensorCaps
        //hmd->DistortionCaps
        // hmd->EyeRenderOrder

		// Distortion capabilities will depend on 'distortionCaps' flags; user should rely on
        // appropriate shaders based on their settings.
        // distortion caps:
        //ovrDistortionCap_Chromatic	= 0x01,		//	Supports chromatic aberration correction.
        //ovrDistortionCap_TimeWarp	= 0x02,		//	Supports timewarp.
        //ovrDistortionCap_Vignette	= 0x08		//	Supports vignetting around the edges of the view.
        unsigned int distortionCaps = ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette;
           
		ovrGLConfig apiConfig;
		apiConfig.OGL.Header.API = ovrRenderAPI_OpenGL;
		apiConfig.OGL.Header.BackBufferSize = hmd->Resolution;
		apiConfig.OGL.Header.Multisample = multisample;
		/// HWND The optional window handle. If unset, rendering will use the current window.
		apiConfig.OGL.Window = window;
		/// HDC The optional device context. If unset, rendering will use a new context.
		apiConfig.OGL.DC = dc;
		
		ovrFovPort eyeFovIn[2];
        ovrEyeRenderDesc eyeRenderDescOut[2];

		// configure per eye:
        for ( int eyeNum = 0; eyeNum < 2; eyeNum++ ) {
			// derive eye configuration for given desired FOV:
			eyeFovIn[eyeNum] = fullview ? hmd->MaxEyeFov[eyeNum] : hmd->DefaultEyeFov[eyeNum];	// or MaxEyeFov?
		}
		ovrHmd_ConfigureRendering(hmd, &apiConfig.Config, distortionCaps, eyeFovIn, eyeRenderDescOut);
        
        // now configure per eye:
        for ( int eyeNum = 0; eyeNum < 2; eyeNum++ ) {
            
            // derive eye configuration for given desired FOV:
            ovrFovPort& fovPort = eyeFovIn[eyeNum];
            ovrEyeRenderDesc& eyeDesc = eyeRenderDescOut[eyeNum];
            
            // get **recommended** texture dim
            // depends on the FOV and desired pixel quality
            // maybe want to scale this up to a POT texture dim?
            ovrSizei texDim = ovrHmd_GetFovTextureSize(hmd, (ovrEyeType)eyeNum, fovPort, 1.0f);
            atom_setlong(a  , texDim.w);
            atom_setlong(a+1, texDim.h);
            outlet_anything(outlet_eye[eyeNum], gensym("texdim"), 2, a);
            
            atom_setlong(a+0, eyeDesc.DistortedViewport.Pos.x);
            atom_setlong(a+1, eyeDesc.DistortedViewport.Pos.y);
            atom_setlong(a+2, eyeDesc.DistortedViewport.Size.w);
            atom_setlong(a+3, eyeDesc.DistortedViewport.Size.h);
            outlet_anything(outlet_eye[eyeNum], gensym("DistortedViewport"), 4, a);
            
            atom_setfloat(a+0, eyeDesc.PixelsPerTanAngleAtCenter.x);
            atom_setfloat(a+1, eyeDesc.PixelsPerTanAngleAtCenter.y);
            outlet_anything(outlet_eye[eyeNum], gensym("PixelsPerTanAngleAtCenter"), 2, a);
            
            atom_setfloat(a+0, eyeDesc.HmdToEyeViewOffset.x);
            atom_setfloat(a+1, eyeDesc.HmdToEyeViewOffset.y);
            atom_setfloat(a+2, eyeDesc.HmdToEyeViewOffset.z);
            outlet_anything(outlet_eye[eyeNum], gensym("ViewAdjust"), 3, a);
            
            // Field Of View (FOV) in tangent of the angle units.
            // As an example, for a standard 90 degree vertical FOV, we would
            // have: { UpTan = tan(90 degrees / 2), DownTan = tan(90 degrees / 2) }.
            // Which means these values are suitable for glFrustum and its ilk
            atom_setfloat(a+0, -eyeDesc.Fov.LeftTan);
            atom_setfloat(a+1,  eyeDesc.Fov.RightTan);
            atom_setfloat(a+2, -eyeDesc.Fov.DownTan);
            atom_setfloat(a+3,  eyeDesc.Fov.UpTan);
            outlet_anything(outlet_eye[eyeNum], gensym("FOV"), 4, a);
            
            // call ovrHmd_GetRenderScaleAndOffset to get uvScale and Offset values for rendering.
            ovrVector2f uvScaleOffsetOut[2];
            ovrSizei textureSize;
            ovrRecti renderViewport;
            // (actually it doesn't really matter what dim, so long as the texture size == the viewport size)
            // since we are mapping each eye viewport to a texture, the dims are the same:
            textureSize.w = OCULUS_DEFAULT_TEXTURE_DIM;
            textureSize.h = OCULUS_DEFAULT_TEXTURE_DIM;
            renderViewport.Size.w = textureSize.w; //hmd->Resolution.w/2;
            renderViewport.Size.h = textureSize.h; //hmd->Resolution.h;
            // texture == viewport, no offset required:
            renderViewport.Pos.x = 0;
            renderViewport.Pos.y = 0;
            // derive the transform from mesh UV coordinates to texture coordinates:
            ovrHmd_GetRenderScaleAndOffset(fovPort, textureSize, renderViewport, uvScaleOffsetOut);
            // and output:
            atom_setfloat(a+0, uvScaleOffsetOut[0].x);
            atom_setfloat(a+1, uvScaleOffsetOut[0].y);
            atom_setfloat(a+2, uvScaleOffsetOut[1].x);
            atom_setfloat(a+3, uvScaleOffsetOut[1].y);
            outlet_anything(outlet_eye[eyeNum], gensym("uvScaleAndOffset"), 4, a);
            
            
            // Use ovrHmd_CreateDistortionMesh to generate distortion mesh.
            ovrDistortionMesh mesh;
            if (!ovrHmd_CreateDistortionMesh(hmd, (ovrEyeType)eyeNum, fovPort, distortionCaps, &mesh)) {
                object_error(&ob, "LibOVR failed to create distortion mesh");
            } else {
                // export this distortion mesh to Jitter
                t_jit_matrix_info info;
                
                // create index matrix:
                void * index_mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
                void * index_mat = jit_object_method(index_mat_wrapper, _jit_sym_getmatrix);
                
                // configure matrix:
                int indexCount = mesh.IndexCount;
                jit_matrix_info_default(&info);
                info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
                info.planecount = 1;
                info.type = gensym("long");
                info.dimcount = 1;
                info.dim[0] = indexCount;
                jit_object_method(index_mat, _jit_sym_setinfo_ex, &info);
                
                // copy indices:
                int32_t * index_ptr;
                jit_object_method(index_mat, _jit_sym_getdata, &index_ptr);
                for (int i=0; i<indexCount; i++) {
                    index_ptr[i] = mesh.pIndexData[i];
                }
                
                // output index matrix for mesh:
                atom_setsym(a, jit_attr_getsym(index_mat_wrapper, _jit_sym_name));
                outlet_anything(outlet_mesh[eyeNum], gensym("index_matrix"), 1, a);
                
                // done with index matrix:
                object_release((t_object *)index_mat_wrapper);
                
                
                
                // create geometry matrix:
                void * mesh_mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
                void * mesh_mat = jit_object_method(mesh_mat_wrapper, _jit_sym_getmatrix);
                
                // configure matrix:
                int vertexCount = mesh.VertexCount;
                jit_matrix_info_default(&info);
                info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
                info.planecount = 12;
                info.type = gensym("float32");
                info.dimcount = 1;
                info.dim[0] = vertexCount;
                jit_object_method(mesh_mat, _jit_sym_setinfo_ex, &info);
                
                // copy & translate vertices to Jitter matrix format:
                float * mat_ptr;
                jit_object_method(mesh_mat, _jit_sym_getdata, &mat_ptr);
                for (int i=0; i<vertexCount; i++) {
                    float * cell = mat_ptr + i*12;
                    ovrDistortionVertex& vertex = mesh.pVertexData[i];
                    
                    // planes 0,1,2: xyz
                    cell[0] = vertex.ScreenPosNDC.x;
                    cell[1] = vertex.ScreenPosNDC.y;
                    cell[2] = 0.5; 	// unused
                    
                    // planes 3,4: texcoord0
                    cell[3] = vertex.TanEyeAnglesR.x;
                    cell[4] = vertex.TanEyeAnglesR.y;
                    
                    // planes 5,6,7: normal
                    cell[5] = vertex.VignetteFactor;
                    cell[6] = vertex.TimeWarpFactor;
                    cell[7] = 0.5;	// unused
                    
                    // planes 8,9,10,11: rgba
                    // mapped to TexG and TexB (chromatic aberration)
                    cell[8]  = vertex.TanEyeAnglesG.x;
                    cell[9]  = vertex.TanEyeAnglesG.y;
                    cell[10] = vertex.TanEyeAnglesB.x;
                    cell[11] = vertex.TanEyeAnglesB.y;
                    
                }
                
                // output geometry matrix:
                atom_setsym(a, jit_attr_getsym(mesh_mat_wrapper, _jit_sym_name));
                outlet_anything(outlet_mesh[eyeNum], _jit_sym_jit_matrix, 1, a);
                
                // done with geometry matrix:
                object_release((t_object *)mesh_mat_wrapper);
                
                // done with LibOVR mesh:
                ovrHmd_DestroyDistortionMesh(&mesh);
            }
        }
    }

    void bang() {
        t_atom a[1];
        
        // Health and Safety Warning display state.
        ovrHSWDisplayState hswDisplayState;
        ovrHmd_GetHSWDisplayState(hmd, &hswDisplayState);
        
        // get tracking state:
        ovrTrackingState ts = ovrHmd_GetTrackingState(hmd, ovr_GetTimeInSeconds() + (predict * 0.001));
        
        if (ts.StatusFlags & ovrStatus_OrientationTracked) {
            
            const ovrPoseStatef predicted = ts.HeadPose;
            const ovrQuatf orient = predicted.ThePose.Orientation;
            
            atom_setfloat(quat  , orient.x);
            atom_setfloat(quat+1, orient.y);
            atom_setfloat(quat+2, orient.z);
            atom_setfloat(quat+3, orient.w);
            outlet_list(outlet_q, 0L, 4, quat);
            
            if (hswDisplayState.Displayed) {
                // Dismiss the Health and Safety Warning?
                // Detect a moderate tap on the side of the HMD.
                float x = ts.RawSensorData.Accelerometer.x;
                float y = ts.RawSensorData.Accelerometer.y;
                float z = ts.RawSensorData.Accelerometer.z;
                // Arbitrary value and representing moderate tap on the side of the DK2 Rift.
                if (((x*x)+(y*y)+(z*z)) > 250.f) ovrHmd_DismissHSWDisplay(hmd);
            }
            atom_setlong(a, hswDisplayState.Displayed);
            outlet_anything(outlet_c, ps_warning, 1, a);
            
        }
        
        
        if (ts.StatusFlags & ovrStatus_PositionConnected) {
            // this can be zero if the HMD is outside the camera frustum
            // or is facing away from it
            // or partially occluded
            // or it is moving too fast
            if (ts.StatusFlags & ovrStatus_PositionTracked) {
                outlet_int(outlet_p_info, (t_atom_long)1);
                
                // TODO
                // float frustumHorizontalFOV = hmd->CameraFrustumHFovInRadians;
                // float frustumVerticalFOV = hmd->CameraFrustumVHFovInRadians;
                // float frustumNear = hmd->CameraFrustumNearZInMeters;
                // float frustumFar = hmd->CameraFrustumFarZInMeters;
                
                // axis system of camera has ZX always parallel to ground (presumably by gravity)
                // default origin is 1m in front of the camera (in +Z), but at the same height (even if camera is tilted)
                
                /*
                 typedef struct ovrPoseStatef_
                 {
                 ovrPosef     ThePose;
                 ovrVector3f  AngularVelocity;
                 ovrVector3f  LinearVelocity;
                 ovrVector3f  AngularAcceleration;
                 ovrVector3f  LinearAcceleration;
                 double       TimeInSeconds;         // Absolute time of this state sample.
                 } ovrPoseStatef;
                 */
                
                const ovrPoseStatef predicted = ts.HeadPose;
                const ovrVector3f position = predicted.ThePose.Position;
                
                atom_setfloat(pos  , position.x);
                atom_setfloat(pos+1, position.y);
                atom_setfloat(pos+2, position.z);
                outlet_list(outlet_p, 0L, 3, pos);
                
                // TODO: accessors for these:
                // CameraPose is the pose of the camera relative to the origin
                // LeveledCameraPose is the same but with roll & pitch zeroed out
                
                
            } else {
                outlet_int(outlet_p_info, (t_atom_long)0);
                // is there some kind of predictive interpolation we can use here?
                
                outlet_list(outlet_p, 0L, 3, pos);
            }
        }
        
    }
};

t_max_err oculus_notify(t_oculus *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
    t_symbol *attrname;
    if (msg == _sym_attr_modified) {       // check notification type
        attrname = (t_symbol *)object_method((t_object *)data, _sym_getname);
        object_post((t_object *)x, "changed attr name is %s", attrname->s_name);
    } else {
        object_post((t_object *)x, "notify %s (self %d)", msg->s_name, sender == x);
    }
    return 0;
}

void oculus_recenter(t_oculus * x) {
    if (x->hmd) ovrHmd_RecenterPose(x->hmd);
}

void oculus_dismiss(t_oculus * x) {
    ovrHmd_DismissHSWDisplay(x->hmd);
}

void oculus_bang(t_oculus * x) {
    x->bang();
}

void oculus_doconfigure(t_oculus *x) {
    x->configure();
}

void oculus_configure(t_oculus *x) {
    defer_low(x, (method)oculus_doconfigure, 0, 0, 0);
}

t_max_err oculus_fullview_set(t_oculus *x, t_object *attr, long argc, t_atom *argv) {
    x->fullview = atom_getlong(argv);
    
    oculus_configure(x);
    return 0;
}

void oculus_assist(t_oculus *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) { // inlet
        if (a == 0) {
            sprintf(s, "messages in, bang to report orientation");
        } else {
            sprintf(s, "I am inlet %ld", a);
        }
    } else {	// outlet
        if (a == 0) {
            sprintf(s, "HMD orientation quaternion (list)");
        } else if (a == 1) {
            sprintf(s, "HMD position (list)");
        } else if (a == 2) {
            sprintf(s, "HMD position tracking status (messages)");
        } else if (a == 3) {
            sprintf(s, "HMD left eye properties (messages)");
        } else if (a == 4) {
            sprintf(s, "HMD left eye mesh (jit_matrix)");
        } else if (a == 5) {
            sprintf(s, "HMD right eye properties (messages)");
        } else if (a == 6) {
            sprintf(s, "HMD right eye mesh (jit_matrix)");
        } else if (a == 7) {
            sprintf(s, "HMD properties (messages)");
        } else {
            sprintf(s, "I am outlet %ld", a);
        }
    }
}


void oculus_free(t_oculus *x) {
    x->~t_oculus();
    
    // free resources associated with our obex entry
    //jit_ob3d_free(x);
    max_jit_object_free(x);
}

void *oculus_new(t_symbol *s, long argc, t_atom *argv)
{
    t_oculus *x = NULL;
    //long i;
    long attrstart;
    t_symbol *dest_name_sym = _jit_sym_nothing;
    
    int index = 0;
    if (argc > 0 && atom_gettype(argv) == A_LONG) index = atom_getlong(argv);
    
    if (x = (t_oculus *)object_alloc(oculus_class)) {
        
        // default attrs:
        // initialize in-place:
        x = new (x) t_oculus(index);
        
        // apply attrs:
        attr_args_process(x, argc, argv);
        
        // now configure:
        oculus_configure(x);
    }
    return (x);
}

int C74_EXPORT main(void) {	
    t_class *maxclass;
    void *ob3d;
    
    ps_quat = gensym("quat");
    ps_pos = gensym("pos");
    ps_warning = gensym("warning");
    
    common_symbols_init();
    
    maxclass = class_new("oculus", (method)oculus_new, (method)oculus_free, (long)sizeof(t_oculus), 
                         0L, A_GIMME, 0);
    
    class_addmethod(maxclass, (method)oculus_assist, "assist", A_CANT, 0);
    class_addmethod(maxclass, (method)oculus_notify, "notify", A_CANT, 0);
    
    //class_addmethod(maxclass, (method)oculus_jit_matrix, "jit_matrix", A_GIMME, 0); 
    class_addmethod(maxclass, (method)oculus_bang, "bang", 0);
    class_addmethod(maxclass, (method)oculus_recenter, "recenter", 0);
    class_addmethod(maxclass, (method)oculus_dismiss, "dismiss", 0);
    class_addmethod(maxclass, (method)oculus_configure, "configure", 0);
    
    CLASS_ATTR_FLOAT(maxclass, "predict", 0, t_oculus, predict);
    CLASS_ATTR_LONG(maxclass, "fullview", 0, t_oculus, fullview);
    CLASS_ATTR_LONG(maxclass, "multisample", 0, t_oculus, multisample);
	CLASS_ATTR_LONG_ARRAY(maxclass, "texture_dim", 0, t_oculus, texture_dim, 2);
    CLASS_ATTR_ACCESSORS(maxclass, "fullview", NULL, oculus_fullview_set);
    CLASS_ATTR_STYLE_LABEL(maxclass, "fullview", 0, "onoff", "use default (0) or max (1) field of view");
    
    class_register(CLASS_BOX, maxclass); 
    oculus_class = maxclass;
    return 0;
}
