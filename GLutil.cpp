//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                      Utility File
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <io.h>

#include "GlOgl.h"
#include "Glextensions.h"
#include "OGLTables.h"

// Configuration Variables
ConfigStruct    UserConfig;
ConfigStruct    InternalConfig;

// Extern prototypes
extern unsigned long    NumberOfErrors;

HDC hDC;
static HGLRC hRC;
static HWND hWND;
static struct
{
    FxU16 red[ 256 ];
    FxU16 green[ 256 ];
    FxU16 blue[ 256 ];
} old_ramp;

static BOOL ramp_stored = TRUE;

static BOOL mode_changed = TRUE;

// Functions

void __cdecl GlideMsg( char *szString, ... )
{
    va_list( Arg );
    va_start( Arg, szString );

    FILE *fHandle = fopen( GLIDEFILE, "at" );
    if ( !fHandle )
    {
        return;
    }
    vfprintf( fHandle, szString, Arg );
    fflush( fHandle );
    fclose( fHandle );

    va_end( Arg );
}

void __cdecl Error( char *szString, ... )
{
    va_list( Arg );
    va_start( Arg, szString );

    if ( NumberOfErrors == 0 )
    {
        GenerateErrorFile( );
    }

    FILE *fHandle = fopen( ERRORFILE, "at" );
    if ( ! fHandle )
    {
        return;
    }
    vfprintf( fHandle, szString, Arg );
    fflush( fHandle );
    fclose( fHandle );

    va_end( Arg );
    NumberOfErrors++;
}

void GLErro( char *Funcao )
{
    GLenum Erro = glGetError( );

    if ( Erro != GL_NO_ERROR )
    {
        Error( "%s: OpenGLError = %s\n", Funcao, gluErrorString( Erro ) );
    }
}

static bool SetScreenMode( HWND hWnd, int xsize, int ysize )
{
    HDC     hdc;
    FxU32   bits_per_pixel;
    bool    found;
    DEVMODE DevMode;

    hdc = GetDC( hWnd );
    bits_per_pixel = GetDeviceCaps( hdc, BITSPIXEL );
    ReleaseDC( hWnd, hdc );
    
    found = false;
    DevMode.dmSize = sizeof( DEVMODE );
    
    for ( int i = 0; 
          !found && EnumDisplaySettings( NULL, i, &DevMode ) != false; 
          i++ )
    {
        if ( ( DevMode.dmPelsWidth == (FxU32)xsize ) && 
             ( DevMode.dmPelsHeight == (FxU32)ysize ) && 
             ( DevMode.dmBitsPerPel == bits_per_pixel ) )
        {
            found = true;
        }
    }
    
    return ( found && ChangeDisplaySettings( &DevMode, CDS_RESET|CDS_FULLSCREEN ) == DISP_CHANGE_SUCCESSFUL );
}

static void ResetScreenMode( void )
{
    ChangeDisplaySettings( NULL, 0 );
}

void InitialiseOpenGLWindow( HWND hWnd, int x, int y, int width, int height )
{
    PIXELFORMATDESCRIPTOR   pfd;
    int                     PixFormat;
    unsigned int            BitsPerPixel;
    HWND                    hwnd = (HWND) hWnd;

    if( hwnd == NULL )
    {
        hwnd = GetActiveWindow();
    }

    if ( hwnd == NULL )
    {
        MessageBox( NULL, "NULL window specified", "Error", MB_OK );
        exit( 1 );
    }

    mode_changed = false;

    if ( UserConfig.InitFullScreen )
    {
        SetWindowLong( hwnd, 
                       GWL_STYLE, 
                       WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
        MoveWindow( hwnd, 0, 0, width, height, false );
        mode_changed = SetScreenMode( hwnd, width, height );
    }
    else
    {
       RECT rect;
       rect.left = 0;
       rect.right = width;
       rect.top = 0;
       rect.bottom = height;

       AdjustWindowRectEx( &rect, 
                           GetWindowLong( hwnd, GWL_STYLE ),
                           GetMenu( hwnd ) != NULL,
                           GetWindowLong( hwnd, GWL_EXSTYLE ) );
       MoveWindow( hwnd, 
                   x, y, 
                   x + ( rect.right - rect.left ),
                   y + ( rect.bottom - rect.top ),
                   true );
    }

    hWND = hwnd;

    hDC = GetDC( hwnd );
    BitsPerPixel = GetDeviceCaps( hDC, BITSPIXEL );

    ZeroMemory( &pfd, sizeof( pfd ) );
    pfd.nSize        = sizeof( pfd );
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = BitsPerPixel;
    pfd.cDepthBits   = BitsPerPixel;

    if ( !( PixFormat = ChoosePixelFormat( hDC, &pfd ) ) )
    {
        MessageBox( NULL, "ChoosePixelFormat() failed:  "
                    "Cannot find a suitable pixel format.", "Error", MB_OK );
        exit( 1 );
    } 

    // the window must have WS_CLIPCHILDREN and WS_CLIPSIBLINGS for this call to
    // work correctly, so we SHOULD set this attributes, not doing that yet
    if ( !SetPixelFormat( hDC, PixFormat, &pfd ) )
    {
        MessageBox( NULL, "SetPixelFormat() failed:  "
                    "Cannot set format specified.", "Error", MB_OK );
        exit( 1 );
    } 

    DescribePixelFormat( hDC, PixFormat, sizeof( PIXELFORMATDESCRIPTOR ), &pfd );
    GlideMsg( "ColorBits	= %d\n", pfd.cColorBits );
    GlideMsg( "DepthBits	= %d\n", pfd.cDepthBits );

    if ( pfd.cDepthBits > 16 )
    {
        UserConfig.PrecisionFix = false;
    }

    hRC = wglCreateContext( hDC );
    wglMakeCurrent( hDC, hRC );

    HDC pDC = GetDC( NULL );

    ramp_stored = GetDeviceGammaRamp( pDC, &old_ramp );

    ReleaseDC( NULL, pDC );
}    

void FinaliseOpenGLWindow( void )
{
    if ( ramp_stored )
    {
        HDC pDC = GetDC( NULL );

        BOOL res = SetDeviceGammaRamp( pDC, &old_ramp );

        ReleaseDC( NULL, pDC );
    }

    wglMakeCurrent( NULL, NULL );
    wglDeleteContext( hRC );
    ReleaseDC( hWND, hDC );

    if( mode_changed )
    {
        ResetScreenMode( );
    }
}

void ConvertColorB( GrColor_t GlideColor, FxU8 &R, FxU8 &G, FxU8 &B, FxU8 &A )
{
    switch ( Glide.State.ColorFormat )
    {
    case GR_COLORFORMAT_ARGB:   //0xAARRGGBB
        A = (FxU8)( ( GlideColor & 0xFF000000 ) >> 24 );
        R = (FxU8)( ( GlideColor & 0x00FF0000 ) >> 16 );
        G = (FxU8)( ( GlideColor & 0x0000FF00 ) >>  8 );
        B = (FxU8)( ( GlideColor & 0x000000FF )       );
        break;

    case GR_COLORFORMAT_ABGR:   //0xAABBGGRR
        A = (FxU8)( ( GlideColor & 0xFF000000 ) >> 24 );
        B = (FxU8)( ( GlideColor & 0x00FF0000 ) >> 16 );
        G = (FxU8)( ( GlideColor & 0x0000FF00 ) >>  8 );
        R = (FxU8)( ( GlideColor & 0x000000FF )       );
        break;

    case GR_COLORFORMAT_RGBA:   //0xRRGGBBAA
        R = (FxU8)( ( GlideColor & 0xFF000000 ) >> 24 );
        G = (FxU8)( ( GlideColor & 0x00FF0000 ) >> 16 );
        B = (FxU8)( ( GlideColor & 0x0000FF00 ) >>  8 );
        A = (FxU8)( ( GlideColor & 0x000000FF )       );
        break;

    case GR_COLORFORMAT_BGRA:   //0xBBGGRRAA
        B = (FxU8)( ( GlideColor & 0xFF000000 ) >> 24 );
        G = (FxU8)( ( GlideColor & 0x00FF0000 ) >> 16 );
        R = (FxU8)( ( GlideColor & 0x0000FF00 ) >>  8 );
        A = (FxU8)( ( GlideColor & 0x000000FF )       );
        break;
    }
}

void ConvertColor4B( GrColor_t GlideColor, FxU32 &C )
{
    switch ( Glide.State.ColorFormat )
    {
    case GR_COLORFORMAT_ARGB:   //0xAARRGGBB
        C = GlideColor;
        break;

    case GR_COLORFORMAT_ABGR:   //0xAABBGGRR
        C = ( ( GlideColor & 0xFF00FF00 ) ||
              ( ( GlideColor & 0x00FF0000 ) >> 16 ) ||
              ( ( GlideColor & 0x000000FF ) <<  16 ) );
        break;

    case GR_COLORFORMAT_RGBA:   //0xRRGGBBAA
        C = ( ( ( GlideColor & 0x00FFFFFF ) << 8 ) ||
              ( ( GlideColor & 0xFF000000 ) >> 24 ) );
        break;

    case GR_COLORFORMAT_BGRA:   //0xBBGGRRAA
        C = ( ( ( GlideColor & 0xFF000000 ) >> 24 ) ||
              ( ( GlideColor & 0x00FF0000 ) >>  8 ) ||
              ( ( GlideColor & 0x0000FF00 ) <<  8 ) ||
              ( ( GlideColor & 0x000000FF ) << 24 ) );
        break;
    }
}

GrColor_t ConvertConstantColor( float R, float G, float B, float A )
{
    GrColor_t r = (GrColor_t) R;
    GrColor_t g = (GrColor_t) G;
    GrColor_t b = (GrColor_t) B;
    GrColor_t a = (GrColor_t) A;

    switch ( Glide.State.ColorFormat )
    {
    case GR_COLORFORMAT_ARGB:   //0xAARRGGBB
        return ( a << 24 ) | ( r << 16 ) | ( g << 8 ) | b;

    case GR_COLORFORMAT_ABGR:   //0xAABBGGRR
        return ( a << 24 ) | ( b << 16 ) | ( g << 8 ) | r;

    case GR_COLORFORMAT_RGBA:   //0xRRGGBBAA
        return ( r << 24 ) | ( g << 16 ) | ( b << 8 ) | a;

    case GR_COLORFORMAT_BGRA:   //0xBBGGRRAA
        return ( b << 24 ) | ( g << 16 ) | ( r << 8 ) | a;
    }

    return 0;
}

void ConvertColorF( GrColor_t GlideColor, float &R, float &G, float &B, float &A )
{
    switch ( Glide.State.ColorFormat )
    {
    case GR_COLORFORMAT_ARGB:   //0xAARRGGBB
        A = (float)( ( GlideColor & 0xFF000000 ) >> 24 ) * D1OVER255;
        R = (float)( ( GlideColor & 0x00FF0000 ) >> 16 ) * D1OVER255;
        G = (float)( ( GlideColor & 0x0000FF00 ) >>  8 ) * D1OVER255;
        B = (float)( ( GlideColor & 0x000000FF )       ) * D1OVER255;
        break;

    case GR_COLORFORMAT_ABGR:   //0xAABBGGRR
        A = (float)( ( GlideColor & 0xFF000000 ) >> 24 ) * D1OVER255;
        B = (float)( ( GlideColor & 0x00FF0000 ) >> 16 ) * D1OVER255;
        G = (float)( ( GlideColor & 0x0000FF00 ) >>  8 ) * D1OVER255;
        R = (float)( ( GlideColor & 0x000000FF )       ) * D1OVER255;
        break;

    case GR_COLORFORMAT_RGBA:   //0xRRGGBBAA
        R = (float)( ( GlideColor & 0xFF000000 ) >> 24 ) * D1OVER255;
        G = (float)( ( GlideColor & 0x00FF0000 ) >> 16 ) * D1OVER255;
        B = (float)( ( GlideColor & 0x0000FF00 ) >>  8 ) * D1OVER255;
        A = (float)( ( GlideColor & 0x000000FF )       ) * D1OVER255;
        break;

    case GR_COLORFORMAT_BGRA:   //0xBBGGRRAA
        B = (float)( ( GlideColor & 0xFF000000 ) >> 24 ) * D1OVER255;
        G = (float)( ( GlideColor & 0x00FF0000 ) >> 16 ) * D1OVER255;
        R = (float)( ( GlideColor & 0x0000FF00 ) >>  8 ) * D1OVER255;
        A = (float)( ( GlideColor & 0x000000FF )       ) * D1OVER255;
        break;
    }
}

//*************************************************
FxU32 GetTexSize( const int Lod, const int aspectRatio, const int format )
{
    /*
    ** If the format is one of these:
    ** GR_TEXFMT_RGB_332
    ** GR_TEXFMT_YIQ_422
    ** GR_TEXFMT_ALPHA_8
    ** GR_TEXFMT_INTENSITY_8
    ** GR_TEXFMT_ALPHA_INTENSITY_44
    ** GR_TEXFMT_P_8
    ** Reduces the size by 2
    */
    return nSquareLod[ format > GR_TEXFMT_RSVD1 ][ aspectRatio ][ Lod ];
}

// Calculates the frequency of the processor clock
#pragma optimize( "", off )
float ClockFrequency( void )
{
    __int64 i64_perf_start, 
            i64_perf_freq, 
            i64_perf_end,
            i64_clock_start,
            i64_clock_end;
    double  d_loop_period, 
            d_clock_freq;

    QueryPerformanceFrequency( (LARGE_INTEGER*)&i64_perf_freq );

    QueryPerformanceCounter( (LARGE_INTEGER*)&i64_perf_start );
    i64_perf_end = 0;

    RDTSC( i64_clock_start );
    while( i64_perf_end < ( i64_perf_start + 350000 ) )
    {
        QueryPerformanceCounter( (LARGE_INTEGER*)&i64_perf_end );
    }
    RDTSC( i64_clock_end );

    i64_clock_end -= i64_clock_start;

    d_loop_period = ((double)i64_perf_freq) / 350000.0;
    d_clock_freq = ((double)( i64_clock_end & 0xffffffff )) * d_loop_period;

    return (float)d_clock_freq;
}
#pragma optimize( "", on )

char * FindConfig( char *IniFile, char *IniConfig )
{
    // Cannot return pointer to local buffer, unless
    // static.
    static char Buffer1[ 256 ];
    char    * EqLocation, 
            * Find;
    FILE    * file;

    Find = NULL;
    file = fopen( IniFile, "r" );

    while ( fgets( Buffer1, 255, file ) != NULL )
    {
        if ( ( EqLocation = strchr( Buffer1, '=' ) ) != NULL )
        {       
            if ( !strncmp( Buffer1, IniConfig, EqLocation - Buffer1 ) )
            {
                Find = EqLocation + 1;
                if ( Find[ strlen( Find ) - 1 ] == '\n' )
                {
                    Find[ strlen( Find ) - 1 ] = '\0';
                }
                break;
            }
        }
    }

    fclose( file );

    return Find;
}

void GetOptions( void )
{
    FILE        * IniFile;
    char        * Pointer;
    extern char * OpenGLideVersion;
    char        Path[ 256 ];

    UserConfig.FogEnable                    = true;
    UserConfig.InitFullScreen               = false;
    UserConfig.PrecisionFix                 = true;
    UserConfig.CreateWindow                 = false;
    UserConfig.EnableMipMaps                = false;
    UserConfig.BuildMipMaps                 = false;
    UserConfig.IgnorePaletteChange          = false;
    UserConfig.ARB_multitexture             = true;
    UserConfig.EXT_paletted_texture         = true;
    UserConfig.EXT_texture_env_add          = false;
    UserConfig.EXT_texture_env_combine      = false;
    UserConfig.EXT_vertex_array             = false;
    UserConfig.EXT_fog_coord                = true;
    UserConfig.EXT_blend_func_separate      = false;
    UserConfig.Wrap565to5551                = true;

    UserConfig.TextureMemorySize            = 16;
    UserConfig.FrameBufferMemorySize        = 8;

    UserConfig.Priority                     = 2;

    strcpy( Path, INIFILE );

    GlideMsg( "Configuration file is %s\n", Path );
    
    if ( access( Path, 00 ) == -1 )
    {
        IniFile = fopen( Path, "w" );
        fprintf( IniFile, "Configuration File for OpenGLide\n\n" );
        fprintf( IniFile, "Info:\n" );
        fprintf( IniFile, "Priority goes from 0(HIGH) to 5(IDLE)\n" );
        fprintf( IniFile, "Texture Memory goes from %d to %d\n", OGL_MIN_TEXTURE_BUFFER, OGL_MAX_TEXTURE_BUFFER );
        fprintf( IniFile, "Frame Buffer Memory goes from %d to %d\n", OGL_MIN_FRAME_BUFFER, OGL_MAX_FRAME_BUFFER );
        fprintf( IniFile, "All other fields are boolean with 1(TRUE) and 0(FALSE)\n\n" );
        fprintf( IniFile, "Version=%s\n\n", OpenGLideVersion );
        fprintf( IniFile, "[Options]\n" );
        fprintf( IniFile, "WrapperPriority=%d\n", UserConfig.Priority );
        fprintf( IniFile, "CreateWindow=%d\n", UserConfig.CreateWindow );
        fprintf( IniFile, "InitFullScreen=%d\n", UserConfig.InitFullScreen );
        fprintf( IniFile, "EnableMipMaps=%d\n", UserConfig.EnableMipMaps );
        fprintf( IniFile, "IgnorePaletteChange=%d\n", UserConfig.IgnorePaletteChange );
        fprintf( IniFile, "Wrap565to5551=%d\n", UserConfig.Wrap565to5551 );
        fprintf( IniFile, "EnablePrecisionFix=%d\n", UserConfig.PrecisionFix );
        fprintf( IniFile, "EnableMultiTextureEXT=%d\n", UserConfig.ARB_multitexture );
        fprintf( IniFile, "EnablePaletteEXT=%d\n", UserConfig.EXT_paletted_texture );
        fprintf( IniFile, "EnableVertexArrayEXT=%d\n", UserConfig.EXT_vertex_array );
        fprintf( IniFile, "TextureMemorySize=%d\n", UserConfig.TextureMemorySize );
        fprintf( IniFile, "FrameBufferMemorySize=%d\n", UserConfig.FrameBufferMemorySize );
        fclose( IniFile );
    }
    else
    {
        Pointer = FindConfig( Path, "Version" );
        if ( Pointer && !strcmp( Pointer, OpenGLideVersion ) )
        {
            Pointer = FindConfig( Path, "CreateWindow" );
            UserConfig.CreateWindow = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "InitFullScreen" );
            UserConfig.InitFullScreen = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "EnableMipMaps" );
            UserConfig.EnableMipMaps = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "IgnorePaletteChange" );
            UserConfig.IgnorePaletteChange = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "EnablePrecisionFix" );
            UserConfig.PrecisionFix = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "EnableMultiTextureEXT" );
            UserConfig.ARB_multitexture = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "EnablePaletteEXT" );
            UserConfig.EXT_paletted_texture = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "EnableVertexArrayEXT" );
            UserConfig.EXT_vertex_array = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "TextureMemorySize" );
            UserConfig.TextureMemorySize = atoi( Pointer );
            Pointer = FindConfig( Path, "WrapperPriority" );
            UserConfig.Priority = atoi( Pointer );
            Pointer = FindConfig( Path, "Wrap565to5551" );
            UserConfig.Wrap565to5551 = atoi( Pointer ) ? true : false;
            Pointer = FindConfig( Path, "FrameBufferMemorySize" );
            UserConfig.FrameBufferMemorySize = atoi( Pointer );
        }
        else
        {
            remove( Path );
            GetOptions( );
        }
    }
}

bool ClearAndGenerateLogFile( void )
{
    FILE    * GlideFile;
    char    tmpbuf[ 128 ];

    remove( ERRORFILE );
    GlideFile = fopen( GLIDEFILE, "w" );
    if ( ! GlideFile )
    {
        return false;
    }
    fclose( GlideFile );

    GlideMsg( OGL_LOG_SEPARATE );
    GlideMsg( "OpenGLide Log File\n" );
    GlideMsg( OGL_LOG_SEPARATE );
    GlideMsg( "***** OpenGLide %s *****\n", OpenGLideVersion );
    GlideMsg( OGL_LOG_SEPARATE );
    _strdate( tmpbuf );
    GlideMsg( "Date: %s\n", tmpbuf );
    _strtime( tmpbuf );
    GlideMsg( "Time: %s\n", tmpbuf );
    GlideMsg( OGL_LOG_SEPARATE );
    GlideMsg( OGL_LOG_SEPARATE );
    ClockFreq = ClockFrequency( );
    GlideMsg( "Clock Frequency: %-4.2f Mhz\n", ClockFreq / 1000000.0f );
    GlideMsg( OGL_LOG_SEPARATE );
    GlideMsg( OGL_LOG_SEPARATE );

    return true;
}

void CloseLogFile( void )
{
    char tmpbuf[ 128 ];
    GlideMsg( OGL_LOG_SEPARATE );
    _strtime( tmpbuf );
    GlideMsg( "Time: %s\n", tmpbuf );
    GlideMsg( OGL_LOG_SEPARATE );

#ifdef OGL_DEBUG
    Fps = (float) Frame * ClockFreq / FpsAux;
    GlideMsg( "FPS = %f\n", Fps );
    GlideMsg( OGL_LOG_SEPARATE );
#endif
}

bool GenerateErrorFile( void )
{
    char    tmpbuf[ 128 ];
    FILE    * ErrorFile;

    ErrorFile = fopen( ERRORFILE, "w");
    if( !ErrorFile )
    {
        return false;
    }
    fclose( ErrorFile );

    Error(  OGL_LOG_SEPARATE );
    Error(  "OpenGLide Error File\n");
    Error(  OGL_LOG_SEPARATE );
    _strdate( tmpbuf );
    Error(  "Date: %s\n", tmpbuf );
    _strtime( tmpbuf );
    Error(  "Time: %s\n", tmpbuf );
    Error(  OGL_LOG_SEPARATE );
    Error(  OGL_LOG_SEPARATE );

    return true;
}

// Detect if Processor has MMX Instructions
int DetectMMX( void )
{
    FxU32 Result;

    __asm
    {
        push EAX
        push EDX
        mov EAX, 1
        CPUID
        mov Result, EDX
        pop EDX
        pop EAX
    }

    return Result & 0x00800000;
}

// Copy Blocks of Memory Using MMX
void MMXCopyMemory( void *Dst, void *Src, FxU32 NumberOfBytes )
{
    __asm
    {
        mov ECX, NumberOfBytes
        mov EAX, Src
        mov EDX, Dst
        jmp start
copying:
        MOVQ MM0, [EAX+ECX]
        MOVQ [EDX+ECX], MM0
start:  sub ECX, 8
        jae copying
        EMMS
    }
}

void MMXSetShort( void *Dst, short Value, FxU32 NumberOfBytes )
{
    __asm
    {
        xor EAX, EAX
        mov ECX, NumberOfBytes
        mov AX, Value
        mov EDX, Dst
        movd MM0, EAX
        PUNPCKLWD MM0, MM0
        PUNPCKLWD MM0, MM0
        jmp start
copying:
        MOVQ [EDX+ECX], MM0
start:  sub ECX, 8
        jae copying
        EMMS
    }
}

void MMXCopyByteFlip( void *Dst, void *Src, FxU32 NumberOfBytes )
{
    __asm
    {       
      mov ESI, Src
      mov EDI, Dst
      mov ECX, NumberOfBytes

                // find the number of pixels that fit in blocks
      mov EAX, ECX
      and ECX, ~0xff
      sub EAX, ECX
      push EAX
      test ECX,ECX
      jz mcbf_breakc
                // init. for inner loop
      lea ESI, [ESI + ECX]
      lea EDI, [EDI + ECX - 8]
      neg ECX
align 16
    mcbf_loopc:
      movq MM0, [ESI + ECX]
      movq MM1, MM0
      psrlw MM0, 8
      add ECX, 8
      psllw MM1, 8
      por MM1, MM0
      movq [EDI + ECX], MM1
      jnz mcbf_loopc
      add EDI, 8
      emms
    mcbf_breakc:
      pop ECX
      test ECX, ECX
      jz mcbf_breakd
      and ECX, ~1
      lea ESI, [ESI + ECX]
      lea EDI, [EDI + ECX]
      neg ECX
    mcbf_loopd:     // trailing data, one pixel at a time
      mov AL, [ESI + ECX]
      mov [EDI + ECX + 1], AL 
      mov DL, [ESI + ECX + 1]
      mov [EDI + ECX], DL
      add ECX, 2
      jnz mcbf_loopd
    mcbf_breakd:
    }
}

