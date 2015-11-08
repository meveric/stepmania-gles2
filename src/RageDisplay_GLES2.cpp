#include "global.h"

#define HAS_GLES2

#include "RageDisplay.h"
#include "RageDisplay_GLES2.h"
#include "RageDisplay_OGL_Helpers.h"
using namespace RageDisplay_Legacy_Helpers;

#include "RageUtil.h"
#include "RageFile.h"
#include "RageLog.h"
#include "RageTimer.h"
#include "RageMath.h"
#include "RageTypes.h"
#include "RageUtil.h"
#include "RageSurface.h"
#include "RageSurfaceUtils.h"
#include "RageTextureManager.h"
#include "Foreach.h"

#include "DisplayResolutions.h"

#include "arch/LowLevelWindow/LowLevelWindow.h"

#include "gles.h"
#ifdef NO_GL_FLUSH
#define glFlush()
#endif

#define FlushGLErrors() do { } while( glGetError() != GL_NO_ERROR )
#define DebugFlushGLErrors() FlushGLErrors()
#define AssertNoGLError() \
{ \
	GLenum error = glGetError(); \
	ASSERT_M( error == GL_NO_ERROR, RageDisplay_Legacy_Helpers::GLToString(error) ); \
}
#define DebugAssertNoGLError() {}

#define GLLOG if(0) LOG->Trace
#define UNSUPPORTED(...) LOG->Trace("UNSUPPORTED: " __VA_ARGS__)
static map<unsigned, RenderTarget *> g_mapRenderTargets;

static RenderTarget *g_pCurrentRenderTarget = NULL;
static bool g_bInvertY = false;
static GLuint _projectionUniform;
static GLuint _modelViewUniform;
static GLuint _textureUniform;
static GLuint ENABLE_LIGHTING = 0;
static GLuint ENABLE_TEXTURE = 0;
static GLint VERTEX_ARRAY = 0;
static GLint COLOR_ARRAY = 0;
static GLint TEXCOORD_ARRAY = 0;
static GLint NORMAL_ARRAY = 0;
namespace
{
	RageDisplay::RagePixelFormatDesc
	PIXEL_FORMAT_DESC[NUM_RagePixelFormat] = {
		{
			/* R8G8B8A8 */
			32,
			{ 0xFF000000,
			  0x00FF0000,
			  0x0000FF00,
			  0x000000FF }
		}, {
			/* B8G8R8A8 */
			32,
			{ 0x0000FF00,
			  0x00FF0000,
			  0xFF000000,
			  0x000000FF }
		}, {
			/* R4G4B4A4 */
			16,
			{ 0xF000,
			  0x0F00,
			  0x00F0,
			  0x000F },
		}, {
			/* R5G5B5A1 */
			16,
			{ 0xF800,
			  0x07C0,
			  0x003E,
			  0x0001 },
		}, {
			/* R5G5B5X1 */
			16,
			{ 0xF800,
			  0x07C0,
			  0x003E,
			  0x0000 },
		}, {
			/* R8G8B8 */
			24,
			{ 0xFF0000,
			  0x00FF00,
			  0x0000FF,
			  0x000000 }
		}, {
			/* Paletted */
			8,
			{ 0,0,0,0 } /* N/A */
		}, {
			/* B8G8R8 */
			24,
			{ 0x0000FF,
			  0x00FF00,
			  0xFF0000,
			  0x000000 }
		}, {
			/* A1R5G5B5 */
			16,
			{ 0x7C00,
			  0x03E0,
			  0x001F,
			  0x8000 },
		}, {
			/* X1R5G5B5 */
			16,
			{ 0x7C00,
			  0x03E0,
			  0x001F,
			  0x0000 },
		}
	};

	/* g_GLPixFmtInfo is used for both texture formats and surface formats.
	 * For example, it's fine to ask for a RagePixelFormat_RGB5 texture, but to
	 * supply a surface matching RagePixelFormat_RGB8.  OpenGL will simply
	 * discard the extra bits.
	 *
	 * It's possible for a format to be supported as a texture format but
	 * not as a surface format.  For example, if packed pixels aren't
	 * supported, we can still use GL_RGB5_A1, but we'll have to convert to
	 * a supported surface pixel format first.  It's not ideal, since we'll
	 * convert to RGBA8 and OGL will convert back, but it works fine.
	 */
	struct GLPixFmtInfo_t {
		GLenum internalfmt; /* target format */
		GLenum format; /* target format */
		GLenum type; /* data format */
	} const g_GLPixFmtInfo[NUM_RagePixelFormat] = {
		{
			/* R8G8B8A8 */
			GL_RGBA,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
		}, {
			/* R8G8B8A8 */
			GL_RGBA8,
			GL_BGRA,
			GL_UNSIGNED_BYTE,
		}, {
			/* B4G4R4A4 */
			GL_RGBA,
			GL_RGBA,
			GL_UNSIGNED_SHORT_4_4_4_4,
		}, {
			/* B5G5R5A1 */
			GL_RGBA,
			GL_RGBA,
			GL_UNSIGNED_SHORT_5_5_5_1,
		}, {
			/* B5G5R5 */
			GL_RGBA,
			GL_RGBA,
			GL_UNSIGNED_SHORT_5_5_5_1,
		}, {
			/* B8G8R8 */
			GL_RGB,
			GL_RGB,
			GL_UNSIGNED_BYTE,
		}, {
			/* Paletted */
//			GL_COLOR_INDEX8_EXT,
//			GL_COLOR_INDEX,
//			GL_UNSIGNED_BYTE,
			GL_RGB8,
			GL_BGR,
			GL_UNSIGNED_BYTE,
		}, {
			/* B8G8R8 */
			GL_RGB8,
			GL_BGR,
			GL_UNSIGNED_BYTE,
		}, {
			// TODO: These don't work on ES2. Work out what needs to happen.
			/* A1R5G5B5 (matches D3DFMT_A1R5G5B5) */
			GL_RGB5_A1,
			GL_BGRA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV,
		}, {
			/* X1R5G5B5 */
			GL_RGB5,
			GL_BGRA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV,
		}
	};

	LowLevelWindow *g_pWind;

	void FixLittleEndian()
	{
#if defined(ENDIAN_LITTLE)
		static bool bInitialized = false;
		if (bInitialized)
			return;
		bInitialized = true;

		for( int i = 0; i < NUM_RagePixelFormat; ++i )
		{
			RageDisplay::RagePixelFormatDesc &pf = PIXEL_FORMAT_DESC[i];

			/* OpenGL and RageSurface handle byte formats differently; we need
			 * to flip non-paletted masks to make them line up. */
			if (g_GLPixFmtInfo[i].type != GL_UNSIGNED_BYTE || pf.bpp == 8)
				continue;

			for( int mask = 0; mask < 4; ++mask)
			{
				int m = pf.masks[mask];
				switch( pf.bpp )
				{
				case 24: m = Swap24(m); break;
				case 32: m = Swap32(m); break;
				default:
					 FAIL_M(ssprintf("Unsupported BPP value: %i", pf.bpp));
				}
				pf.masks[mask] = m;
			}
		}
#endif
	}
	namespace Caps
	{
		int iMaxTextureUnits = 1;
		int iMaxTextureSize = 256;
	}
	namespace State
	{
		bool bZTestEnabled = false;
		bool bZWriteEnabled = false;
		bool bAlphaTestEnabled = false;
	}
}
static int g_iMaxTextureUnits = 32;
static bool SetTextureUnit( TextureUnit tu )
{
	GLLOG( "RageDisplay_GLES2::SetTextureUnit(%d)", (int)tu );
	// If multitexture isn't supported, ignore all textures except for 0.
	if ((int) tu > g_iMaxTextureUnits) {
		GLLOG( "RageDisplay_GLES2::SetTextureUnit(%d)", (int)tu );
		return false;
	}
	DebugAssertNoGLError();
	glActiveTexture( enum_add2(GL_TEXTURE0, tu) );
	DebugAssertNoGLError();
	return true;
}

static void SetupVertices( const RageSpriteVertex v[], int iNumVerts )
{
	GLLOG( "RageDisplay_GLES2::SetupVertices(%d)", iNumVerts);
            
	static float *Vertex, *Texture, *Normal;	
	static float *Color;
	static int Size = 0;
	if (iNumVerts > Size)
	{
		Size = iNumVerts;
		delete [] Vertex;
		delete [] Color;
		delete [] Texture;
		delete [] Normal;
		Vertex = new float[Size*3];
		Color = new float[Size*4];
		Texture = new float[Size*2];
		Normal = new float[Size*3];
	}

	for( unsigned i = 0; i < unsigned(iNumVerts); ++i )
	{
		Vertex[i*3+0]  = v[i].p[0];
		Vertex[i*3+1]  = v[i].p[1];
		Vertex[i*3+2]  = v[i].p[2];
		Color[i*4+0]   = v[i].c.r / 256.0;
		Color[i*4+1]   = v[i].c.g / 256.0;
		Color[i*4+2]   = v[i].c.b / 256.0;
		Color[i*4+3]   = v[i].c.a / 256.0;
		Texture[i*2+0] = v[i].t[0];
		Texture[i*2+1] = v[i].t[1];
		Normal[i*3+0] = v[i].n[0];
		Normal[i*3+1] = v[i].n[1];
		Normal[i*3+2] = v[i].n[2];
		GLLOG( " V(%6.4f, %6.4f, %6.4f) C(%d, %d, %d, %d) T(%6.4f, %6.4f)\n",
			v[i].p[0], v[i].p[1], v[i].p[2],
			v[i].c.r, v[i].c.g, v[i].c.b, v[i].c.a,
			v[i].t[0], v[i].t[1]); 
	}
//printf("VERTEX_ARRAY: %d\nCOLOR_ARRAY: %d\n", VERTEX_ARRAY, COLOR_ARRAY);
	glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, 0, (void *)Vertex); 
	glEnableVertexAttribArray(VERTEX_ARRAY);
	DebugAssertNoGLError();

	glEnableVertexAttribArray(COLOR_ARRAY);
	glVertexAttribPointer(COLOR_ARRAY, 4, GL_FLOAT, GL_FALSE, 0, (void *)Color); 
	DebugAssertNoGLError();


	glEnableVertexAttribArray(TEXCOORD_ARRAY);
	glVertexAttribPointer(TEXCOORD_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, ( void *)Texture);
	DebugAssertNoGLError();
/*
	glEnableVertexAttribArray(NORMAL_ARRAY);
	glVertexAttribPointer(NORMAL_ARRAY, 3, GL_FLOAT, GL_FALSE, 0, Normal);
	DebugAssertNoGLError();
*/
}

class InvalidateObject;
static set<InvalidateObject *> g_InvalidateList;
class InvalidateObject
{
public:
	InvalidateObject() { g_InvalidateList.insert( this ); }
	virtual ~InvalidateObject() { g_InvalidateList.erase( this ); }
	virtual void Invalidate() = 0;
};

static void InvalidateObjects()
{
	FOREACHS( InvalidateObject*, g_InvalidateList, it )
		(*it)->Invalidate();
}

GLhandleARB CompileShader( GLenum ShaderType, RString sFile, vector<RString> asDefines )
{
	RString sBuffer;
	{
		RageFile file;
		if (!file.Open(sFile))
		{
			LOG->Warn( "Error compiling shader %s: %s", sFile.c_str(), file.GetError().c_str() );
			return 0;
		}
		
		if (file.Read(sBuffer, file.GetFileSize()) == -1)
		{
			LOG->Warn( "Error compiling shader %s: %s", sFile.c_str(), file.GetError().c_str() );
			return 0;
		}
	}

	GLLOG( "Compiling shader %s", sFile.c_str() );
	GLhandleARB hShader = glCreateShader( ShaderType );
	vector<const GLcharARB *> apData;
	vector<GLint> aiLength;
	FOREACH( RString, asDefines, s )
	{
		*s = ssprintf( "#define %s\n", s->c_str() );
		apData.push_back( s->data() );
		aiLength.push_back( s->size() );
	}
	apData.push_back( "#line 1\n" );
	aiLength.push_back( 8 );

	apData.push_back( sBuffer.data() );
	aiLength.push_back( sBuffer.size() );
	glShaderSource( hShader, apData.size(), &apData[0], &aiLength[0] );

	glCompileShader( hShader );
	GLint bCompileStatus  = GL_FALSE;
	char msg[512];
	glGetShaderInfoLog(hShader, sizeof msg, NULL, msg);
	glGetShaderiv(hShader, GL_COMPILE_STATUS, &bCompileStatus);
	if (!bCompileStatus)
	{

		LOG->Warn( "Error compiling shader %s:\n%s", sFile.c_str(), msg );
		glDeleteShader( hShader );
		return 0;
	}

	GLLOG( "Messages compiling shader %s:\n%s", sFile.c_str(), msg );

	return hShader;
}

GLhandleARB LoadShader( GLenum ShaderType, RString sFile, vector<RString> asDefines )
{
	GLLOG( "RageDisplay_GLES2::LoadShader(%d, %s)", ShaderType, sFile.c_str());
	// XXX: dumb, but I don't feel like refactoring ragedisplay for this. -Colby
	GLhandleARB secondaryShader = 0;
	if (sFile == "Data/Shaders/GLSL/Cel.vert")
		secondaryShader = CompileShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Cel.frag", asDefines);
	else if (sFile == "Data/Shaders/GLSL/Shell.vert")
		secondaryShader = CompileShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Shell.frag", asDefines);
	else if (sFile == "Data/Shaders/GLSL/Default.vert")
		secondaryShader = CompileShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Default.frag", asDefines);
	
	GLhandleARB hShader = CompileShader( ShaderType, sFile, asDefines );
	if (hShader == 0)
		return 0;

	GLhandleARB hProgram = glCreateProgram();
	glAttachShader( hProgram, hShader );
	
	if (secondaryShader)
	{
		glAttachShader( hProgram, secondaryShader );
		glDeleteShader( secondaryShader );
	}
	glDeleteShader( hShader );

	// Link the program.
	glLinkProgram( hProgram );
	GLint bLinkStatus = false;
	glGetProgramiv(hProgram, GL_LINK_STATUS, &bLinkStatus);
	if (!bLinkStatus)
	{
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(hProgram, 1000, &len, log);

		LOG->Warn( "Error linking shader %s: %s", sFile.c_str(), log);
		glDeleteProgram( hProgram );
		return 0;
	}
	return hProgram;
}
static int g_iAttribTextureMatrixScale;

static GLhandleARB g_gDefaultShader = 0;
static GLhandleARB g_bUnpremultiplyShader = 0;
static GLhandleARB g_bColorBurnShader = 0;
static GLhandleARB g_bColorDodgeShader = 0;
static GLhandleARB g_bVividLightShader = 0;
static GLhandleARB g_hHardMixShader = 0;
static GLhandleARB g_hOverlayShader = 0;
static GLhandleARB g_hScreenShader = 0;
static GLhandleARB g_hYUYV422Shader = 0;
static GLhandleARB g_gShellShader = 0;
static GLhandleARB g_gCelShader = 0;
static GLhandleARB g_bTextureMatrixShader = 0;

void InitShaders()
{
	GLLOG( "InitShaders()");
	// xxx: replace this with a ShaderManager or something that reads in
	// the shaders and determines shader type by file extension. -aj
	// argh shaders in stepmania are painful -colby
	vector<RString> asDefines;
	
	g_gDefaultShader = LoadShader(	GL_VERTEX_SHADER_ARB, "Data/Shaders/GLSL/Default.vert", asDefines );

	// used for scrolling textures (I think)
	g_bTextureMatrixShader = LoadShader(	GL_VERTEX_SHADER_ARB, "Data/Shaders/GLSL/Texture matrix scaling.vert", asDefines );
	
	// these two are for dancing characters and are both actually shader pairs
	g_gShellShader = LoadShader(			GL_VERTEX_SHADER_ARB, "Data/Shaders/GLSL/Shell.vert", asDefines );
	g_gCelShader = LoadShader(			GL_VERTEX_SHADER_ARB, "Data/Shaders/GLSL/Cel.vert", asDefines );
	
	// effects
	g_bUnpremultiplyShader	= LoadShader(	GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Unpremultiply.frag", asDefines );
	g_bColorBurnShader	= LoadShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Color burn.frag", asDefines );
	g_bColorDodgeShader	= LoadShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Color dodge.frag", asDefines );
	g_bVividLightShader		= LoadShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Vivid light.frag", asDefines );
	g_hHardMixShader		= LoadShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Hard mix.frag", asDefines );
	g_hOverlayShader		= LoadShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Overlay.frag", asDefines );
	g_hScreenShader		= LoadShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/Screen.frag", asDefines );
	g_hYUYV422Shader		= LoadShader( GL_FRAGMENT_SHADER_ARB, "Data/Shaders/GLSL/YUYV422.frag", asDefines );
	
	// Bind attributes.
	if (g_bTextureMatrixShader)
	{
		FlushGLErrors();
		g_iAttribTextureMatrixScale = glGetAttribLocationARB( g_bTextureMatrixShader, "TextureMatrixScale" );
		if (g_iAttribTextureMatrixScale == -1)
		{
			GLLOG( "Scaling shader link failed: couldn't bind attribute \"TextureMatrixScale\"" );
			glDeleteShader( g_bTextureMatrixShader );
			g_bTextureMatrixShader = 0;
		}
		else
		{
			AssertNoGLError();

			/* Older Catalyst drivers seem to throw GL_INVALID_OPERATION here. */
			glVertexAttrib2fARB( g_iAttribTextureMatrixScale, 1, 1 );
			GLenum iError = glGetError();
			if (iError == GL_INVALID_OPERATION)
			{
				GLLOG( "Scaling shader failed: glVertexAttrib2fARB returned GL_INVALID_OPERATION" );
				glDeleteShader( g_bTextureMatrixShader );
				g_bTextureMatrixShader = 0;
			}
			else
			{
				ASSERT_M( iError == GL_NO_ERROR, GLToString(iError) );
			}
		}
	}
}

RageDisplay_GLES2::RageDisplay_GLES2()
{
	GLLOG( "RageDisplay_GLES2::RageDisplay_GLES2()" );
	LOG->MapLog("renderer", "Current renderer: OpenGL ES 2.0");

	FixLittleEndian();
	RageDisplay_Legacy_Helpers::Init();

	g_pWind = NULL;
}

RString
RageDisplay_GLES2::Init( const VideoModeParams &p, bool bAllowUnacceleratedRenderer )
{
	g_pWind = LowLevelWindow::Create();

	bool bIgnore = false;
	RString sError = SetVideoMode( p, bIgnore );
	if (sError != "")
		return sError;

	// Get GPU capabilities up front so we don't have to query later.
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &Caps::iMaxTextureSize );
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &Caps::iMaxTextureUnits );

	// Log driver details
	g_pWind->LogDebugInformation();
	LOG->Info( "OGL Vendor: %s", glGetString(GL_VENDOR) );
	LOG->Info( "OGL Renderer: %s", glGetString(GL_RENDERER) );
	LOG->Info( "OGL Version: %s", glGetString(GL_VERSION) );
	LOG->Info( "OGL Max texture size: %i", Caps::iMaxTextureSize );
	LOG->Info( "OGL Texture units: %i", Caps::iMaxTextureUnits );

	/* Pretty-print the extension string: */
	LOG->Info( "OGL Extensions:" );

	/* Log this, so if people complain that the radar looks bad on their
	 * system we can compare them: */
	//glGetFloatv( GL_LINE_WIDTH_RANGE, g_line_range );
	//glGetFloatv( GL_POINT_SIZE_RANGE, g_point_range );

	return RString();
}

// Return true if mode change was successful.
// bNewDeviceOut is set true if a new device was created and textures
// need to be reloaded.
RString RageDisplay_GLES2::TryVideoMode( const VideoModeParams &p, bool &bNewDeviceOut )
{
	VideoModeParams vm = p;
	vm.windowed = 1; // force windowed until I trust this thing.
	LOG->Warn( "RageDisplay_GLES2::TryVideoMode( %d, %d, %d, %d, %d, %d )",
		vm.windowed, vm.width, vm.height, vm.bpp, vm.rate, vm.vsync );

	RString err = g_pWind->TryVideoMode( vm, bNewDeviceOut );
	if (err != "")
		return err;	// failed to set video mode

	if (bNewDeviceOut)
	{
		/* We have a new OpenGL context, so we have to tell our textures that
		 * their OpenGL texture number is invalid. */
		if (TEXTUREMAN)
			TEXTUREMAN->InvalidateTextures();

		/* Delete all render targets.  They may have associated resources other than
		 * the texture itself. */
		FOREACHM( unsigned, RenderTarget *, g_mapRenderTargets, rt )
			delete rt->second;
		g_mapRenderTargets.clear();

		/* Recreate all vertex buffers. */
		InvalidateObjects();

		InitShaders();
	}

	ResolutionChanged();

	return RString();
}

int RageDisplay_GLES2::GetMaxTextureSize() const
{
	return Caps::iMaxTextureSize;
}

bool RageDisplay_GLES2::BeginFrame()
{
	/* We do this in here, rather than ResolutionChanged, or we won't update the
	 * viewport for the concurrent rendering context. */
	int fWidth = g_pWind->GetActualVideoModeParams().width;
	int fHeight = g_pWind->GetActualVideoModeParams().height;
	DebugAssertNoGLError();
	glViewport( 0, 0, fWidth, fHeight );
	GLLOG( "RageDisplay_GLES2::BeginFrame() %dx%d", fWidth, fHeight );

	glClearColor( 0.0f, 0.2f, 0.0f, 1.0f );
	SetZWrite( true );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	DebugAssertNoGLError();
	SetEffectMode(EffectMode_Normal);

	return RageDisplay::BeginFrame();
}

void RageDisplay_GLES2::EndFrame()
{
	glFlush();

	GLLOG( "RageDisplay_GLES2::EndFrame()" );
	// XXX: This is broken on NVidia, as their xrandr sucks.
	//FrameLimitBeforeVsync( g_pWind->GetActualVideoModeParams().rate );
	g_pWind->SwapBuffers();
	//FrameLimitAfterVsync();

	g_pWind->Update();

	RageDisplay::EndFrame();
}

RageDisplay_GLES2::~RageDisplay_GLES2()
{
	delete g_pWind;
}

void
RageDisplay_GLES2::GetDisplayResolutions( DisplayResolutions &out ) const
{
	out.clear();
	g_pWind->GetDisplayResolutions( out );
}

RageSurface*
RageDisplay_GLES2::CreateScreenshot()
{
	const RagePixelFormatDesc &desc = PIXEL_FORMAT_DESC[RagePixelFormat_RGB8];
	RageSurface *image = CreateSurface(
		640, 480, desc.bpp,
		desc.masks[0], desc.masks[1], desc.masks[2], desc.masks[3] );

	memset( image->pixels, 0, 480*image->pitch );

	return image;
}

const RageDisplay::RagePixelFormatDesc*
RageDisplay_GLES2::GetPixelFormatDesc(RagePixelFormat pf) const
{
	ASSERT( pf >= 0 && pf < NUM_RagePixelFormat );
	return &PIXEL_FORMAT_DESC[pf];
}

RageMatrix
RageDisplay_GLES2::GetOrthoMatrix( float l, float r, float b, float t, float zn, float zf )
{
	RageMatrix m(
		2/(r-l),      0,            0,           0,
		0,            2/(t-b),      0,           0,
		0,            0,            -2/(zf-zn),   0,
		-(r+l)/(r-l), -(t+b)/(t-b), -(zf+zn)/(zf-zn),  1 );
	return m;
}

class RageCompiledGeometryGLES2 : public RageCompiledGeometry
{
public:
	
	void Allocate( const vector<msMesh> &vMeshes )
	{
		// TODO
		GLLOG( "RageCompiledGeometryGLES2::Allocate()" );
	}
	void Change( const vector<msMesh> &vMeshes )
	{
		// TODO
		GLLOG( "RageCompiledGeometryGLES2::Change()" );
	}
	void Draw( int iMeshIndex ) const
	{
		// TODO
		GLLOG( "RageCompiledGeometryGLES2::Draw()" );
	}
};

RageCompiledGeometry*
RageDisplay_GLES2::CreateCompiledGeometry()
{

	GLLOG( "RageDisplay_GLES2::CreateCompiledGeometry()");
	return new RageCompiledGeometryGLES2;
}

void
RageDisplay_GLES2::DeleteCompiledGeometry( RageCompiledGeometry *p )
{
	GLLOG( "RageDisplay_GLES2::CompiledGeometry()");
	delete p;
}

RString
RageDisplay_GLES2::GetApiDescription() const
{
	return "OpenGL ES 2.0";
}

VideoModeParams
RageDisplay_GLES2::GetActualVideoModeParams() const
{
	return g_pWind->GetActualVideoModeParams();
}

void
RageDisplay_GLES2::SetBlendMode( BlendMode mode )
{
	// TODO
	GLLOG( "RageDisplay_GLES2::SetBlendMode(%s)",
              mode == BLEND_INVERT_DEST ? "BLEND_INVERT_DEST" :
              mode == BLEND_SUBTRACT ? "BLEND_SUBTRACT" :
              mode == BLEND_NORMAL ? "BLEND_NORMAL" :
              mode == BLEND_ADD ? "BLEND_ADD" :
              mode == BLEND_MODULATE ? "BLEND_MODULATE" :
              mode == BLEND_COPY_SRC ? "BLEND_COPY_SRC" :
              mode == BLEND_ALPHA_MASK ? "BLEND_ALPHA_MASK" :
              mode == BLEND_ALPHA_KNOCK_OUT ? "BLEND_ALPHA_KNOCK_OUT" :
              mode == BLEND_ALPHA_MULTIPLY ? "BLEND_ALPHA_MULTIPLY" :
              mode == BLEND_WEIGHTED_MULTIPLY ? "BLEND_WEIGHTED_MULTIPLY" :
              mode == BLEND_NO_EFFECT ? "BLEND_NO_EFFECT" :
              "UNKNOWN");
	glEnable(GL_BLEND);

	if (mode == BLEND_INVERT_DEST)
		glBlendEquation( GL_FUNC_SUBTRACT );
	else if (mode == BLEND_SUBTRACT)
		glBlendEquation( GL_FUNC_REVERSE_SUBTRACT );
	else
		glBlendEquation( GL_FUNC_ADD );

	int iSourceRGB, iDestRGB;
	int iSourceAlpha = GL_ONE, iDestAlpha = GL_ONE_MINUS_SRC_ALPHA;
	switch( mode )
	{
	case BLEND_NORMAL:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case BLEND_ADD:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_ONE;
		break;
	case BLEND_SUBTRACT:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case BLEND_MODULATE:
		iSourceRGB = GL_ZERO; iDestRGB = GL_SRC_COLOR;
		break;
	case BLEND_COPY_SRC:
		iSourceRGB = GL_ONE; iDestRGB = GL_ZERO;
		iSourceAlpha = GL_ONE; iDestAlpha = GL_ZERO;
		break;
	case BLEND_ALPHA_MASK:
		iSourceRGB = GL_ZERO; iDestRGB = GL_ONE;
		iSourceAlpha = GL_ZERO; iDestAlpha = GL_SRC_ALPHA;
		break;
	case BLEND_ALPHA_KNOCK_OUT:
		iSourceRGB = GL_ZERO; iDestRGB = GL_ONE;
		iSourceAlpha = GL_ZERO; iDestAlpha = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case BLEND_ALPHA_MULTIPLY:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_ZERO;
		break;
	case BLEND_WEIGHTED_MULTIPLY:
		/* output = 2*(dst*src).  0.5,0.5,0.5 is identity; darker colors darken the image,
		 * and brighter colors lighten the image. */
		iSourceRGB = GL_DST_COLOR; iDestRGB = GL_SRC_COLOR;
		break;
	case BLEND_INVERT_DEST:
		/* out = src - dst.  The source color should almost always be #FFFFFF, to make it "1 - dst". */
		iSourceRGB = GL_ONE; iDestRGB = GL_ONE;
		break;
	case BLEND_NO_EFFECT:
		iSourceRGB = GL_ZERO; iDestRGB = GL_ONE;
		iSourceAlpha = GL_ZERO; iDestAlpha = GL_ONE;
		break;
	DEFAULT_FAIL( mode );
	}

	glBlendFuncSeparate( iSourceRGB, iDestRGB, iSourceAlpha, iDestAlpha );
}

bool
RageDisplay_GLES2::SupportsTextureFormat( RagePixelFormat pixfmt, bool realtime )
{
	GLLOG( "RageDisplay_GLES2::SupportsTextureFormat()" );
	/* If we support a pixfmt for texture formats but not for surface formats, then
	 * we'll have to convert the texture to a supported surface format before uploading.
	 * This is too slow for dynamic textures. */
	if (realtime && !SupportsSurfaceFormat(pixfmt))
		return false;

	switch (g_GLPixFmtInfo[pixfmt].format)
	{
	//case GL_BGR:
	case GL_BGRA:
		//return !!GLEW_EXT_bgra;
		return false; // no BGRA on ES2 (without exts)
	default:
		return true;
	}

	return true;
}

bool
RageDisplay_GLES2::SupportsPerVertexMatrixScale()
{
	return true;
}

RagePixelFormat RageDisplay_GLES2::GetImgPixelFormat( RageSurface* &img, bool &bFreeImg, int width, int height, bool bPalettedTexture )
{
	GLLOG( "RageDisplay_GLES2::GetImgPixelFormat()" );
	RagePixelFormat pixfmt = FindPixelFormat( img->format->BitsPerPixel, img->format->Rmask, img->format->Gmask, img->format->Bmask, img->format->Amask );
	
	/* If img is paletted, we're setting up a non-paletted texture, and color indexes
	 * are too small, depalettize. */
	bool bSupported = true;
	if (!bPalettedTexture && img->fmt.BytesPerPixel == 1)
		bSupported = false;

	if (pixfmt == RagePixelFormat_Invalid || !SupportsSurfaceFormat(pixfmt))
		bSupported = false;

	if (!bSupported)
	{
		/* The source isn't in a supported, known pixel format.  We need to convert
		 * it ourself.  Just convert it to RGBA8, and let OpenGL convert it back
		 * down to whatever the actual pixel format is.  This is a very slow code
		 * path, which should almost never be used. */
		pixfmt = RagePixelFormat_RGBA8;
		ASSERT( SupportsSurfaceFormat(pixfmt) );

		const RagePixelFormatDesc *pfd = DISPLAY->GetPixelFormatDesc(pixfmt);

		RageSurface *imgconv = CreateSurface( img->w, img->h,
			pfd->bpp, pfd->masks[0], pfd->masks[1], pfd->masks[2], pfd->masks[3] );
		RageSurfaceUtils::Blit( img, imgconv, width, height );
		img = imgconv;
		bFreeImg = true;
	}
	else
	{
		bFreeImg = false;
	}

	return pixfmt;
}
unsigned
RageDisplay_GLES2::CreateTexture(
	RagePixelFormat pixfmt,
	RageSurface* pImg,
	bool bGenerateMipMaps
	)
{
	// TODO
	GLLOG( "RageDisplay_GLES2::CreateTexture(generateMipmaps=%d)", bGenerateMipMaps );
	ASSERT( pixfmt < NUM_RagePixelFormat );
	ASSERT( pixfmt != RagePixelFormat_PAL);

	bool bFreeImg;
	RagePixelFormat SurfacePixFmt = GetImgPixelFormat( pImg, bFreeImg, pImg->w, pImg->h, pixfmt == RagePixelFormat_PAL );
	ASSERT( SurfacePixFmt != RagePixelFormat_Invalid );

	GLenum glTexFormat = g_GLPixFmtInfo[pixfmt].internalfmt;
	GLenum glImageFormat = g_GLPixFmtInfo[SurfacePixFmt].format;
	GLenum glImageType = g_GLPixFmtInfo[SurfacePixFmt].type;

	/* If the image is paletted, but we're not sending it to a paletted image,
	 * set up glPixelMap. */
	//SetPixelMapForSurface( glImageFormat, glTexFormat, pImg->format->palette ); //FIXME

	SetTextureUnit( TextureUnit_1 );

	// allocate OpenGL texture resource
	unsigned int iTexHandle;
	glGenTextures( 1, reinterpret_cast<GLuint*>(&iTexHandle) );
	ASSERT( iTexHandle != 0 );
	
	glBindTexture( GL_TEXTURE_2D, iTexHandle );

	SetTextureFiltering( TextureUnit_1, true );
	SetTextureWrapping( TextureUnit_1, false );

	//glPixelStorei( GL_UNPACK_ROW_LENGTH, pImg->pitch / pImg->format->BytesPerPixel ); //FIXME
	DebugAssertNoGLError();

#if 0 //FIXME
	if (pixfmt == RagePixelFormat_PAL)
	{
		/* The texture is paletted; set the texture palette. */
		GLubyte palette[256*4];
		memset( palette, 0, sizeof(palette) );
		int p = 0;
		/* Copy the palette to the format OpenGL expects. */
		for( int i = 0; i < pImg->format->palette->ncolors; ++i )
		{
			palette[p++] = pImg->format->palette->colors[i].r;
			palette[p++] = pImg->format->palette->colors[i].g;
			palette[p++] = pImg->format->palette->colors[i].b;
			palette[p++] = pImg->format->palette->colors[i].a;
		}

		/* Set the palette. */
		glColorTableEXT( GL_TEXTURE_2D, GL_RGBA8, 256, GL_RGBA, GL_UNSIGNED_BYTE, palette );

		GLint iRealFormat = 0;
		glGetColorTableParameterivEXT( GL_TEXTURE_2D, GL_COLOR_TABLE_FORMAT, &iRealFormat );
		ASSERT( iRealFormat == GL_RGBA8 );
	}
#endif

	GLLOG( "%s (format %s, %ix%i, format %s, type %s, pixfmt %i, imgpixfmt %i) => %d",
		bGenerateMipMaps? "gluBuild2DMipmaps":"glTexImage2D",
		GLToString(glTexFormat).c_str(),
		pImg->w, pImg->h,
		GLToString(glImageFormat).c_str(),
		GLToString(glImageType).c_str(), pixfmt, SurfacePixFmt, iTexHandle );

	DebugFlushGLErrors();
	DebugAssertNoGLError();

	glTexImage2D(
		GL_TEXTURE_2D, 0, glTexFormat, 
		power_of_two(pImg->w), power_of_two(pImg->h), 0,
		glImageFormat, glImageType, NULL );
	DebugAssertNoGLError();
	if (pImg->pixels) {
		glTexSubImage2D( GL_TEXTURE_2D, 0,
			0, 0,
			pImg->w, pImg->h,
			glImageFormat, glImageType, pImg->pixels );
		DebugAssertNoGLError();
		if (bGenerateMipMaps) {
			glGenerateMipmap(GL_TEXTURE_2D);
			DebugAssertNoGLError();
		}
	}

	/* Sanity check: */
	//if (pixfmt == RagePixelFormat_PAL)
	//{
	//	GLint iSize = 0;
	//	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GLenum(GL_TEXTURE_INDEX_SIZE_EXT), &iSize );
	//	if (iSize != 8)
	//		RageException::Throw( "Thought paletted textures worked, but they don't." );
	//}

	//glPixelStorei( GL_UNPACK_ALIGNMENT, 0 ); //FIXME
	glFlush();
	DebugAssertNoGLError();

	if (bFreeImg)
		delete pImg;
	return iTexHandle;
}

void
RageDisplay_GLES2::UpdateTexture( 
	unsigned iTexHandle, 
	RageSurface* img,
	int xoffset, int yoffset, int width, int height 
	)
{
	// TODO
	GLLOG( "RageDisplay_GLES2::UpdateTexture()" );
}

void
RageDisplay_GLES2::DeleteTexture( unsigned iTexHandle )
{
	// TODO
	GLLOG( "RageDisplay_GLES2::DeleteTexture()" );
}

void
RageDisplay_GLES2::ClearAllTextures()
{
	GLLOG( "RageDisplay_GLES2::ClearAllTextures()" );
	DebugAssertNoGLError();
	FOREACH_ENUM( TextureUnit, i )
		SetTexture( i, 0 );

	// HACK:  Reset the active texture to 0.
	// TODO:  Change all texture functions to take a stage number.
	glActiveTexture(GL_TEXTURE0);
}

int
RageDisplay_GLES2::GetNumTextureUnits()
{
	GLLOG( "RageDisplay_GLES2::GetNumTextureUnits() == %d", Caps::iMaxTextureUnits);
	return Caps::iMaxTextureUnits;
}

void
RageDisplay_GLES2::SetTexture( TextureUnit tu, unsigned iTexture )
{
	//TODO
	GLLOG( "RageDisplay_GLES2::SetTexture(%d, %d)", (int)tu, iTexture);
	DebugAssertNoGLError();
	if (!SetTextureUnit( tu ))
		return;
	if (iTexture)
	{
		glBindTexture( GL_TEXTURE_2D, iTexture );
	}
	glUniform1i(ENABLE_TEXTURE, iTexture);
	DebugAssertNoGLError();

}

void 
RageDisplay_GLES2::SetTextureMode( TextureUnit tu, TextureMode tm )
{
	// TODO
	GLLOG( "RageDisplay_GLES2::SetTextureMode(%d, %d)", (int)tu, (int)tm );
	if (!SetTextureUnit( tu ))
		return;
}

void
RageDisplay_GLES2::SetTextureWrapping( TextureUnit tu, bool b )
{
	// TODO
	GLLOG( "RageDisplay_GLES2::SetTextureWrapping(%d, %s)", tu, b ? "GL_REPEAT" : "GL_CLAMP_TO_EDGE" );
	/* This should be per-texture-unit state, but it's per-texture state in OpenGl,
	 * so we'll behave incorrectly if the same texture is used in more than one texture
	 * unit simultaneously with different wrapping. */
	SetTextureUnit( tu );
	
	GLenum mode = b ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode );
	DebugAssertNoGLError();
}

void
RageDisplay_GLES2::SetTextureFiltering( TextureUnit tu, bool b )
{
	GLLOG( "RageDisplay_GLES2::SetTextureFiltering(%d, %s)", (int)tu, b ? "GL_LINEAR" : "GL_NEAREST" );
	DebugAssertNoGLError();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, b ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, b ? GL_LINEAR : GL_NEAREST);
	DebugAssertNoGLError();
#if 0
	
	GLint iMinFilter = 0;
	if (b)
	{
		GLint iWidth1 = -1;
		GLint iWidth2 = -1;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iWidth1);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &iWidth2);
		if (iWidth1 > 1 && iWidth2 != 0)
		{
			/* Mipmaps are enabled. */
			if (g_pWind->GetActualVideoModeParams().bTrilinearFiltering)
				iMinFilter = GL_LINEAR_MIPMAP_LINEAR;
			else
				iMinFilter = GL_LINEAR_MIPMAP_NEAREST;
		}
		else
		{
			iMinFilter = GL_LINEAR;
		}
	}
	else
	{
		iMinFilter = GL_NEAREST;
	}

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iMinFilter );
#endif
}

void RageDisplay_GLES2::SetEffectMode( EffectMode effect )
{
	static int last_effect_mode = -1;
	//TODO
	GLLOG( "RageDisplay_GLES2::SetEffectMode(%d)", (int)effect );
	if(last_effect_mode == effect)
		return;

	GLhandleARB hShader = 0;
	effect = EffectMode_Normal; //FIXME
	switch (effect)
	{
	case EffectMode_Normal:		hShader = g_gDefaultShader; break;
	case EffectMode_Unpremultiply:	hShader = g_bUnpremultiplyShader; break;
	case EffectMode_ColorBurn:	hShader = g_bColorBurnShader; break;
	case EffectMode_ColorDodge:	hShader = g_bColorDodgeShader; break;
	case EffectMode_VividLight:	hShader = g_bVividLightShader; break;
	case EffectMode_HardMix:	hShader = g_hHardMixShader; break;
	case EffectMode_Overlay:	hShader = g_hOverlayShader; break;
	case EffectMode_Screen:	hShader = g_hScreenShader; break;
	case EffectMode_YUYV422:	hShader = g_hYUYV422Shader; break;
	}

	DebugFlushGLErrors();
	if (hShader == 0)
		return;
	glUseProgram( hShader );
	last_effect_mode = effect;
	if (effect == EffectMode_Normal) {
		VERTEX_ARRAY = glGetAttribLocation(hShader, "Position");
		COLOR_ARRAY = glGetAttribLocation(hShader, "SourceColor");
		TEXCOORD_ARRAY = glGetAttribLocation(hShader, "TexCoord");
		ENABLE_LIGHTING = glGetUniformLocation(hShader, "enable_lighting");
                ENABLE_TEXTURE  = glGetUniformLocation(hShader, "enable_texture");
		_projectionUniform = glGetUniformLocation(hShader, "Projection");
		_modelViewUniform = glGetUniformLocation(hShader, "Modelview");
		_textureUniform = glGetUniformLocation(hShader, "Texture");
/*	printf("ENABLE_LIGHTING: %d\n", ENABLE_LIGHTING);
	printf("_projectionUniform: %d\n", _projectionUniform);
	printf("_modelViewUniform: %d\n", _modelViewUniform);
*/
		DebugAssertNoGLError();
		return;
	}
	
	GLint iTexture1 = glGetUniformLocation( hShader, "Texture1" );
	GLint iTexture2 = glGetUniformLocation( hShader, "Texture2" );
	glUniform1i( iTexture1, 0 );
	glUniform1i( iTexture2, 1 );

	//if (effect == EffectMode_YUYV422)
	//{
	//	GLint iTextureWidthUniform = glGetUniformLocation( hShader, "TextureWidth" );
	//	GLint iWidth;
	//	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iWidth );
	//	glUniform1i( iTextureWidthUniform, iWidth );
	//}

	DebugAssertNoGLError();
}

bool RageDisplay_GLES2::IsEffectModeSupported( EffectMode effect )
{
	GLLOG( "RageDisplay_GLES2::IsEffectModeSupported(%d)", (int)effect );
	switch( effect )
	{
	case EffectMode_Normal:		return true;
	case EffectMode_Unpremultiply:	return g_bUnpremultiplyShader != 0;
	case EffectMode_ColorBurn:	return g_bColorBurnShader != 0;
	case EffectMode_ColorDodge:	return g_bColorDodgeShader != 0;
	case EffectMode_VividLight:	return g_bVividLightShader != 0;
	case EffectMode_HardMix:	return g_hHardMixShader != 0;
	case EffectMode_Overlay:		return g_hOverlayShader != 0;
	case EffectMode_Screen:		return g_hScreenShader != 0;
	case EffectMode_YUYV422:	return g_hYUYV422Shader != 0;
	}

	return false;
}
bool
RageDisplay_GLES2::IsZWriteEnabled() const
{
	return State::bZWriteEnabled;
}

bool
RageDisplay_GLES2::IsZTestEnabled() const
{
	return State::bZTestEnabled;
}

void
RageDisplay_GLES2::SetZWrite( bool b )
{
	if (State::bZWriteEnabled != b)
	{
		State::bZWriteEnabled = b;
		glDepthMask( b );
	}
	DebugAssertNoGLError();
}

void
RageDisplay_GLES2::SetZBias( float f )
{
	float fNear = SCALE( f, 0.0f, 1.0f, 0.05f, 0.0f );
	float fFar = SCALE( f, 0.0f, 1.0f, 1.0f, 0.95f );

	//glDepthRange( fNear, fFar );
}

void
RageDisplay_GLES2::SetZTestMode( ZTestMode mode )
{
	glEnable( GL_DEPTH_TEST );
	switch( mode )
	{
	case ZTEST_OFF:
		glDisable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		State::bZTestEnabled = false;
		break;
	case ZTEST_WRITE_ON_PASS: glDepthFunc( GL_LEQUAL ); break;
	case ZTEST_WRITE_ON_FAIL: glDepthFunc( GL_GREATER ); break;
	default:
		FAIL_M(ssprintf("Invalid ZTestMode: %i", mode));
	}
	State::bZTestEnabled = true;
	DebugAssertNoGLError();
}

/*
bool RageDisplay_Legacy::IsZWriteEnabled() const
{
	bool a;
	glGetBooleanv( GL_DEPTH_WRITEMASK, (unsigned char*)&a );
	return a;
}
*/

void
RageDisplay_GLES2::ClearZBuffer()
{
	bool write = IsZWriteEnabled();
	SetZWrite( true );
	glClear( GL_DEPTH_BUFFER_BIT );
	SetZWrite( write );
	DebugAssertNoGLError();
}

void
RageDisplay_GLES2::SetCullMode( CullMode mode )
{
	if (mode != CULL_NONE)
		glEnable(GL_CULL_FACE);
	switch( mode )
	{
	case CULL_BACK:
		glCullFace( GL_BACK );
		break;
	case CULL_FRONT:
		glCullFace( GL_FRONT );
		break;
	case CULL_NONE:
		glDisable( GL_CULL_FACE );
		break;
	default:
		FAIL_M(ssprintf("Invalid CullMode: %i", mode));
	}
	DebugAssertNoGLError();
}

void
RageDisplay_GLES2::SetAlphaTest( bool b )
{
	//TODO
	GLLOG( "RageDisplay_GLES2::SetAlphaTest(%d)", b ? 1 : 0 );
/*
	void (*toggle)(GLenum) = b ? glEnable : glDisable;
	if (State::bAlphaTestEnabled != b)
	{
		State::bAlphaTestEnabled = b;
		toggle(GL_ALPHA_TEST);
	}
*/
}

void
RageDisplay_GLES2::SetMaterial( 
	const RageColor &emissive,
	const RageColor &ambient,
	const RageColor &diffuse,
	const RageColor &specular,
	float shininess
	)
{
	// TODO
	GLLOG( "RageDisplay_GLES2::SetMaterial()" );
}

void
RageDisplay_GLES2::SetLineWidth(float fWidth)
{
	GLLOG( "RageDisplay_GLES2::SetLineWidth(%f)", fWidth);
	glLineWidth(fWidth);
	DebugAssertNoGLError();
}

unsigned RageDisplay_GLES2::CreateRenderTarget( const RenderTargetParam &param, int &iTextureWidthOut, int &iTextureHeightOut )
{
	GLLOG( "RageDisplay_GLES2::CreateRenderTarget()");
	RenderTarget *pTarget;
	pTarget = g_pWind->CreateRenderTarget();

	pTarget->Create( param, iTextureWidthOut, iTextureHeightOut );

	unsigned iTexture = pTarget->GetTexture();

	ASSERT( g_mapRenderTargets.find(iTexture) == g_mapRenderTargets.end() );
	g_mapRenderTargets[iTexture] = pTarget;
	return iTexture;
}

void RageDisplay_GLES2::SetRenderTarget( unsigned iTexture, bool bPreserveTexture )
{
	GLLOG( "RageDisplay_GLES2::SetRenderTarget()");
	if (iTexture == 0)
	{
		g_bInvertY = false;
		glFrontFace( GL_CCW );
		
		/* Pop matrixes affected by SetDefaultRenderStates. */
		DISPLAY->CameraPopMatrix();

		/* Reset the viewport. */
		int fWidth = g_pWind->GetActualVideoModeParams().width;
		int fHeight = g_pWind->GetActualVideoModeParams().height;
		glViewport( 0, 0, fWidth, fHeight );

		if (g_pCurrentRenderTarget)
			g_pCurrentRenderTarget->FinishRenderingTo();
		g_pCurrentRenderTarget = NULL;
		return;
	}

	/* If we already had a render target, disable it. */
	if (g_pCurrentRenderTarget != NULL)
		SetRenderTarget(0, true);

	/* Enable the new render target. */
	ASSERT(g_mapRenderTargets.find(iTexture) != g_mapRenderTargets.end());
	RenderTarget *pTarget = g_mapRenderTargets[iTexture];
	pTarget->StartRenderingTo();
	g_pCurrentRenderTarget = pTarget;

	/* Set the viewport to the size of the render target. */
	glViewport(0, 0, pTarget->GetParam().iWidth, pTarget->GetParam().iHeight);

	/* If this render target implementation flips Y, compensate.   Inverting will
	 * switch the winding order. */
	g_bInvertY = pTarget->InvertY();
	if (g_bInvertY)
		glFrontFace(GL_CW);

	/* The render target may be in a different OpenGL context, so re-send
	 * state.  Push matrixes affected by SetDefaultRenderStates. */
	DISPLAY->CameraPushMatrix();
	SetDefaultRenderStates();

	/* Clear the texture, if requested.  Always set the associated state, for
	 * consistency. */
	glClearColor(0,0,0,0);
	SetZWrite(true);

	/* If bPreserveTexture is false, clear the render target.  Only clear the depth
	 * buffer if the target has one; otherwise we're clearing the real depth buffer. */
	if (!bPreserveTexture)
	{
		int iBit = GL_COLOR_BUFFER_BIT;
		if (pTarget->GetParam().bWithDepthBuffer)
			iBit |= GL_DEPTH_BUFFER_BIT;
		glClear(iBit);
	}
}
void
RageDisplay_GLES2::SetPolygonMode(PolygonMode pm)
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::SetPolygonMode(%d)", pm);
#if 0
	GLenum m;
	switch (pm)
	{
	case POLYGON_FILL:	m = GL_FILL; break;
	case POLYGON_LINE:	m = GL_LINE; break;
	default:
		FAIL_M(ssprintf("Invalid PolygonMode: %i", pm));
	}
	glPolygonMode(GL_FRONT_AND_BACK, m);
#endif
}

void
RageDisplay_GLES2::SetLighting( bool b )
{
	GLLOG( "RageDisplay_GLES2::SetLighting(%d)", b );
	glUniform1i(ENABLE_LIGHTING, b);
	DebugAssertNoGLError();
}

void
RageDisplay_GLES2::SetLightOff( int index )
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::SetLightOff()" );
}

void
RageDisplay_GLES2::SetLightDirectional( 
	int index, 
	const RageColor &ambient, 
	const RageColor &diffuse, 
	const RageColor &specular, 
	const RageVector3 &dir )
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::SetLightDirectional()" );
}

void
RageDisplay_GLES2::SetSphereEnvironmentMapping( TextureUnit tu, bool b )
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::SetSphereEnvironmentMapping()" );
}

void
RageDisplay_GLES2::SetCelShaded( int stage )
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::SetCelShaded()");
}

void RageDisplay_GLES2::SendCurrentMatrices()
{
	RageMatrix projection;
	RageMatrixMultiply( &projection, GetCentering(), GetProjectionTop() );

	if (g_bInvertY)
	{
		RageMatrix flip;
		RageMatrixScale( &flip, +1, -1, +1 );
		RageMatrixMultiply( &projection, &flip, &projection );
	}
	glUniformMatrix4fv(_projectionUniform, 1, GL_FALSE, (GLfloat *)&projection);

	// OpenGL has just "modelView", whereas D3D has "world" and "view"
	RageMatrix modelView;
	RageMatrixMultiply( &modelView, GetViewTop(), GetWorldTop() );
        glUniformMatrix4fv(_modelViewUniform, 1, GL_FALSE, (GLfloat *)&modelView);

	//glMatrixMode( GL_TEXTURE );
	//glLoadMatrixf( (const float*)GetTextureTop() );
        glUniformMatrix4fv(_textureUniform, 1, GL_FALSE, (GLfloat *)GetTextureTop());
}

void
RageDisplay_GLES2::DrawQuadsInternal( const RageSpriteVertex v[], int iNumVerts )
{
	GLLOG( "RageDisplay_GLES2::DrawQuadsInternal() %i", iNumVerts);
	DebugAssertNoGLError();
	SendCurrentMatrices();
	SetupVertices( v, iNumVerts );
	//GLfloat vertices[] = {-1, -1, 0, //bottom left corner
        //              -1,  1, 0, //top left corner
        //               1,  1, 0, //top right corner
        //               1, -1, 0}; // bottom right rocner

	//GLubyte indices[] = {0,1,2, // first triangle (bottom left - top left - top right)
        //             0,2,3}; // second triangle (bottom left - top right - bottom right)


	// there isn't a quad primitive in GLES, so we have to fake it with indexed triangles
	int iNumQuads = iNumVerts/4;
	int iNumTriangles = iNumQuads*2;
	int iNumIndices = iNumTriangles*3;

	// make a temporary index buffer
	static vector<GLubyte> vIndices;
	unsigned uOldSize = vIndices.size();
	unsigned uNewSize = max(uOldSize,(unsigned)iNumIndices);
	vIndices.resize( uNewSize );
	for( uint16_t i=(uint16_t)uOldSize/6; i<(uint16_t)iNumQuads; i++ )
	{
		vIndices[i*6+0] = i*4+0;
		vIndices[i*6+1] = i*4+1;
		vIndices[i*6+2] = i*4+2;
		vIndices[i*6+3] = i*4+0;
		vIndices[i*6+4] = i*4+2;
		vIndices[i*6+5] = i*4+3;
	}
	DebugAssertNoGLError();
	glDrawElements(GL_TRIANGLES, iNumIndices, GL_UNSIGNED_BYTE, &vIndices[0]);
	DebugAssertNoGLError();
}

void
RageDisplay_GLES2::DrawQuadStripInternal( const RageSpriteVertex v[], int iNumVerts )
{
	// TODO
	GLLOG( "RageDisplay_GLES2::DrawQuadStripInternal()");
	// there isn't a quad strip primitive in GLESv2, so we have to fake it with indexed triangles
	int iNumQuads = (iNumVerts-2)/2;
	int iNumTriangles = iNumQuads*2;
	int iNumIndices = iNumTriangles*3;

	// make a temporary index buffer
	static vector<GLubyte> vIndices;
	unsigned uOldSize = vIndices.size();
	unsigned uNewSize = max(uOldSize,(unsigned)iNumIndices);
	vIndices.resize( uNewSize );
	for( uint16_t i=(uint16_t)uOldSize/6; i<(uint16_t)iNumQuads; i++ )
	{
		vIndices[i*6+0] = i*2+0;
		vIndices[i*6+1] = i*2+1;
		vIndices[i*6+2] = i*2+2;
		vIndices[i*6+3] = i*2+1;
		vIndices[i*6+4] = i*2+2;
		vIndices[i*6+5] = i*2+3;
	}

	SendCurrentMatrices();
	SetupVertices( v, iNumVerts );
	glDrawElements( 
		GL_TRIANGLES, 
		iNumIndices,
		GL_UNSIGNED_BYTE, 
		&vIndices[0] );
}

void
RageDisplay_GLES2::DrawFanInternal( const RageSpriteVertex v[], int iNumVerts )
{
	GLLOG( "RageDisplay_GLES2::DrawFanInternal()");
	SendCurrentMatrices();

	SetupVertices( v, iNumVerts );
	glDrawArrays( GL_TRIANGLE_FAN, 0, iNumVerts );
}

void
RageDisplay_GLES2::DrawStripInternal( const RageSpriteVertex v[], int iNumVerts )
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::DrawStripInternal()");
}

void
RageDisplay_GLES2::DrawTrianglesInternal( const RageSpriteVertex v[], int iNumVerts )
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::DrawTrianglesInternal()");
}

void
RageDisplay_GLES2::DrawCompiledGeometryInternal( const RageCompiledGeometry *p, int 
	iMeshIndex )
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::DrawCompiledGeometryInternal()");
}

void
RageDisplay_GLES2::DrawLineStripInternal( const RageSpriteVertex v[], int iNumVerts, float LineWidth )
{
	// TODO
	UNSUPPORTED( "RageDisplay_GLES2::DrawLineStripInternal()");
}

void
RageDisplay_GLES2::DrawSymmetricQuadStripInternal( const RageSpriteVertex v[], int iNumVerts )
{
	GLLOG( "RageDisplay_GLES2::DrawSymmetricQuadStripInternal()");
	int iNumPieces = (iNumVerts-3)/3;
	int iNumTriangles = iNumPieces*4;
	int iNumIndices = iNumTriangles*3;

	// make a temporary index buffer
	static vector<GLubyte> vIndices;
	unsigned uOldSize = vIndices.size();
	unsigned uNewSize = max(uOldSize,(unsigned)iNumIndices);
	vIndices.resize( uNewSize );
	for( uint16_t i=(uint16_t)uOldSize/12; i<(uint16_t)iNumPieces; i++ )
	{
		// { 1, 3, 0 } { 1, 4, 3 } { 1, 5, 4 } { 1, 2, 5 }
		vIndices[i*12+0] = i*3+1;
		vIndices[i*12+1] = i*3+3;
		vIndices[i*12+2] = i*3+0;
		vIndices[i*12+3] = i*3+1;
		vIndices[i*12+4] = i*3+4;
		vIndices[i*12+5] = i*3+3;
		vIndices[i*12+6] = i*3+1;
		vIndices[i*12+7] = i*3+5;
		vIndices[i*12+8] = i*3+4;
		vIndices[i*12+9] = i*3+1;
		vIndices[i*12+10] = i*3+2;
		vIndices[i*12+11] = i*3+5;
	}

	SendCurrentMatrices();

	SetupVertices( v, iNumVerts );
	glDrawElements( 
		GL_TRIANGLES, 
		iNumIndices,
		GL_UNSIGNED_BYTE, 
		&vIndices[0] );
}

bool
RageDisplay_GLES2::SupportsSurfaceFormat( RagePixelFormat pixfmt )
{

	if ((g_GLPixFmtInfo[pixfmt].internalfmt == GL_RGBA || g_GLPixFmtInfo[pixfmt].internalfmt == GL_RGB)
            && (g_GLPixFmtInfo[pixfmt].type == GL_UNSIGNED_BYTE || g_GLPixFmtInfo[pixfmt].type == GL_UNSIGNED_SHORT_5_6_5
                || g_GLPixFmtInfo[pixfmt].type ==  GL_UNSIGNED_SHORT_4_4_4_4 || g_GLPixFmtInfo[pixfmt].type == GL_UNSIGNED_SHORT_5_5_5_1))
        {
            return true;
        }
        return false;
}

/*
 * Copyright (c) 2012 Colby Klein
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
