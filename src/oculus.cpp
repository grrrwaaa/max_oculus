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

#define OCULUS_DEFAULT_TEXTURE_DIM 2048

#ifdef __APPLE__
	#include "CoreFoundation/CoreFoundation.h"
    #define MY_PLUGIN_BUNDLE_IDENTIFIER "com.cycling74.oculus"
#endif

t_class *oculus_class;
static t_symbol * ps_quat;

char bundle_path[MAX_PATH_CHARS];

volatile int libcount = 0;

class t_oculus {
public:
	t_object	ob;			// the object itself (must be first)
	
	int			fullview;
	
	// many outlets:
	void *		outlet_q;
	void *		outlet_eye[2];
	void *		outlet_mesh[2];
	void *		outlet_c;
	
	// the quaternion orientation of the HMD:
	t_atom		quat[4];
	
	// LibOVR objects:
	ovrHmd		hmd;
	ovrHmdDesc	hmdDesc;
	
	t_oculus(int index = 0) {
		t_atom a[1];
		
		fullview = 1;
		
		libcount++;
		if (libcount == 1 && !ovr_Initialize()) {
			object_error(&ob, "LibOVR: failed to initialize library");
			return;
		}
		
		int hmd_count = ovrHmd_Detect();
		object_post(&ob, "%d HMDs detected", hmd_count);
		if (hmd_count > 0 && index < hmd_count) {
			hmd = ovrHmd_Create(index);
		}
		hmd = ovrHmd_Create(index);
		if (hmd == 0) {
			object_warn(&ob, "LibOVR: HMD not detected, using offline DK1 simulator instead");
			hmd = ovrHmd_CreateDebug(ovrHmd_DK1);
		}
		ovrHmd_GetDesc(hmd, &hmdDesc);
		
		// start tracking:
		// supported capabilities -- use hmdDesc by default
		// required capabilities -- use none by default
		if (!ovrHmd_StartSensor(hmd, hmdDesc.SensorCaps, 0)) {
			object_error(&ob, "failed to start LibOVR sensor");
			ovrSensorDesc desc;
			if (ovrHmd_GetSensorDesc(hmd, &desc)) {
				atom_setsym(a, gensym(desc.SerialNumber));
				outlet_anything(outlet_c, gensym("serial"), 1, a);
			}
		}
	}

	~t_oculus() {
	
		if (hmd) {
			ovrHmd_StopSensor(hmd);
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
		
		#define HMD_CASE(T) case T: { \
			atom_setsym(a, gensym( #T )); \
			outlet_anything(outlet_c, gensym("hmdType"), 1, a); \
			break; \
		}
		switch(hmdDesc.Type) {
			HMD_CASE(ovrHmd_DK1)
			HMD_CASE(ovrHmd_DKHD)
			HMD_CASE(ovrHmd_CrystalCoveProto)
			HMD_CASE(ovrHmd_DK2)
			default: {
				atom_setsym(a, gensym("unknown")); 
				outlet_anything(outlet_c, gensym("hmdType"), 1, a);
			}
		}
		#undef HMD_CASE
		
		atom_setsym(a, gensym(hmdDesc.Manufacturer)); 
		outlet_anything(outlet_c, gensym("Manufacturer"), 1, a);
		atom_setsym(a, gensym(hmdDesc.ProductName)); 
		outlet_anything(outlet_c, gensym("ProductName"), 1, a);
		atom_setsym(a, gensym(hmdDesc.DisplayDeviceName)); 
		outlet_anything(outlet_c, gensym("DisplayDeviceName"), 1, a);
		atom_setlong(a, hmdDesc.Resolution.w); 
		atom_setlong(a+1, hmdDesc.Resolution.h); 
		outlet_anything(outlet_c, gensym("Resolution"), 2, a);

		// capabilities:
		//hmdDesc.HmdCaps
		//hmdDesc.SensorCaps
		//hmdDesc.DistortionCaps
		// hmdDesc.EyeRenderOrder
		
		// now configure per eye:
		for ( int eyeNum = 0; eyeNum < 2; eyeNum++ ) {
		
			// derive eye configuration for given desired FOV:
			ovrFovPort fovPort = fullview ? hmdDesc.MaxEyeFov[eyeNum] : hmdDesc.DefaultEyeFov[eyeNum];	// or MaxEyeFov?
			ovrEyeRenderDesc eyeDesc = ovrHmd_GetRenderDesc(hmd, (ovrEyeType)eyeNum, fovPort);
			
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
			
			atom_setfloat(a+0, eyeDesc.ViewAdjust.x);
			atom_setfloat(a+1, eyeDesc.ViewAdjust.y);
			atom_setfloat(a+2, eyeDesc.ViewAdjust.z);
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
			renderViewport.Size.w = textureSize.w; //hmdDesc.Resolution.w/2;
			renderViewport.Size.h = textureSize.h; //hmdDesc.Resolution.h;
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
			// Distortion capabilities will depend on 'distortionCaps' flags; user should rely on
			// appropriate shaders based on their settings.
			// distortion caps:
			//ovrDistortionCap_Chromatic	= 0x01,		//	Supports chromatic aberration correction.
			//ovrDistortionCap_TimeWarp	= 0x02,		//	Supports timewarp.
			//ovrDistortionCap_Vignette	= 0x08		//	Supports vignetting around the edges of the view.
			unsigned int distortionCaps = ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette;
			if (!ovrHmd_CreateDistortionMesh(hmd, (ovrEyeType)eyeNum, fovPort, distortionCaps, &mesh)) {
				object_error(&ob, "LibOVR failed to create distortion mesh");
			} else {
				// export this distortion mesh to Jitter
				//output_mesh(&mesh, eyeNum);
				
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
					cell[0] = vertex.Pos.x;
					cell[1] = vertex.Pos.y;
					cell[2] = 0.5; 	// unused
					
					// planes 3,4: texcoord0
					cell[3] = vertex.TexR.x;
					cell[4] = vertex.TexR.y;
					
					// planes 5,6,7: normal
					cell[5] = vertex.VignetteFactor; 
					cell[6] = vertex.TimeWarpFactor;
					cell[7] = 0.5;	// unused
					
					// planes 8,9,10,11: rgba
					// mapped to TexG and TexB (chromatic aberration)
					cell[8]  = vertex.TexG.x;
					cell[9]  = vertex.TexG.y;
					cell[10] = vertex.TexB.x;
					cell[11] = vertex.TexB.y;
				
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
		//static unsigned int frameIndex = 0;
		if (hmd) {
		
			ovrSensorState ss = ovrHmd_GetSensorState(hmd, ovr_GetTimeInSeconds());
			
			ovrPoseStatef predicted = ss.Predicted;
			ovrQuatf orient = predicted.Pose.Orientation;
			
			atom_setfloat(quat  , orient.x);
			atom_setfloat(quat+1, orient.y);
			atom_setfloat(quat+2, orient.z);
			atom_setfloat(quat+3, orient.w);
			outlet_list(outlet_q, 0L, 4, quat); 
			
			// This whole section is for the purpose of computing a predicted orientation
			// but it requires taking over control of rendering in the max patch
			
			/*
			
			//  3. Use ovrHmd_BeginFrameTiming, ovrHmd_GetEyePose and ovrHmd_BeginFrameTiming
			//     in the rendering loop to obtain timing and predicted view orientation for
			//     each eye.
			//      - If relying on timewarp, use ovr_WaitTillTime after rendering+flush, followed
			//        by ovrHmd_GetEyeTimewarpMatrices to obtain timewarp matrices used 
			//        in distortion pixel shader to reduce latency.
			
			
			//To facilitate prediction, ovrHmd_GetSensorState takes absolute time, in seconds, as a second argument. This is the same value as reported by the ovr_GetTimeInSeconds global function. 
//			In a production application, however, you should use one of the real-time computed values returned by ovrHmd_BeginFrame or ovrHmd_BeginFrameTiming. Prediction is covered in more detail in the section on Frame Timing.
//			
			ovrHmd_GetFrameTiming(hmd, ++frameIndex);
			ovrFrameTiming hmdFrameTiming = ovrHmd_BeginFrameTiming(hmd, frameIndex);
		
			for ( int eyeNum = 0; eyeNum < 2; eyeNum++ ) {
				ovrEyeType eye = hmdDesc.EyeRenderOrder[eyeNum];
				ovrPosef eyePose = ovrHmd_GetEyePose(hmd, eye);
				
				// Computes timewarp matrices used by distortion mesh shader, these are used to adjust
				// for orientation change since the last call to ovrHmd_GetEyePose for this eye.
				// The ovrDistortionVertex::TimeWarpFactor is used to blend between the matrices,
				// usually representing two different sides of the screen.
				// Must be called on the same thread as ovrHmd_BeginFrameTiming.
				//OVR_EXPORT void     ovrHmd_GetEyeTimewarpMatrices(ovrHmd hmd, ovrEyeType eye, ovrPosef renderPose, ovrMatrix4f twmOut[2]);
				
				atom_setfloat(quat  , eyePose.Orientation.x);
				atom_setfloat(quat+1, eyePose.Orientation.y);
				atom_setfloat(quat+2, eyePose.Orientation.z);
				atom_setfloat(quat+3, eyePose.Orientation.w);
				outlet_anything(outlet_c, gensym("eye"), 4, quat); 
			}
			
			// Marks the end of game-rendered frame, tracking the necessary timing information. This
			// function must be called immediately after Present/SwapBuffers + GPU sync. GPU sync is important
			// before this call to reduce latency and ensure proper timing.
			// TODO: how to make this fire *after* sync?
			ovrHmd_EndFrameTiming(hmd);
			*/
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
			sprintf(s, "HMD left eye properties (messages)"); 
		} else if (a == 2) {
			sprintf(s, "HMD left eye mesh (jit_matrix)"); 
		} else if (a == 3) {
			sprintf(s, "HMD right eye properties (messages)"); 
		} else if (a == 4) {
			sprintf(s, "HMD right eye mesh (jit_matrix)");  
		} else if (a == 5) {
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
	
	if (x = (t_oculus *)object_alloc(oculus_class)) {
		
		x->outlet_c = outlet_new(x, 0);
		x->outlet_mesh[1] = outlet_new(x, "jit_matrix");
		x->outlet_eye[1] = outlet_new(x, 0);
		x->outlet_mesh[0] = outlet_new(x, "jit_matrix");
		x->outlet_eye[0] = outlet_new(x, 0);
		x->outlet_q = listout(x);
		
		// default attrs:
		// initialize in-place:
		x = new (x) t_oculus();
		
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
	
	#ifdef __APPLE__
		CFBundleRef mainBundle = CFBundleGetBundleWithIdentifier(CFSTR(MY_PLUGIN_BUNDLE_IDENTIFIER));
		CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
		if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)bundle_path, MAX_PATH_CHARS))
		{
			// error!
		}
		CFRelease(resourcesURL);
		
		printf("bundle path %s\n", bundle_path);
	#endif
	
	common_symbols_init();
	
	maxclass = class_new("oculus", (method)oculus_new, (method)oculus_free, (long)sizeof(t_oculus), 
				  0L, A_GIMME, 0);
				  
	class_addmethod(maxclass, (method)oculus_assist, "assist", A_CANT, 0);
	class_addmethod(maxclass, (method)oculus_notify, "notify", A_CANT, 0);
	
	//class_addmethod(maxclass, (method)oculus_jit_matrix, "jit_matrix", A_GIMME, 0); 
	class_addmethod(maxclass, (method)oculus_bang, "bang", 0);
	class_addmethod(maxclass, (method)oculus_configure, "configure", 0);

	CLASS_ATTR_LONG(maxclass, "fullview", 0, t_oculus, fullview);
	CLASS_ATTR_ACCESSORS(maxclass, "fullview", NULL, oculus_fullview_set);
	CLASS_ATTR_STYLE_LABEL(maxclass, "fullview", 0, "onoff", "use default (0) or max (1) field of view");
	
	class_register(CLASS_BOX, maxclass); 
	oculus_class = maxclass;
	return 0;
}
