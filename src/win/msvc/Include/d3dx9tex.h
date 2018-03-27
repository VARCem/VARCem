//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) Microsoft Corporation.  All Rights Reserved.
//
//  File:       d3dx9tex.h
//  Content:    D3DX texturing APIs
//
//////////////////////////////////////////////////////////////////////////////

//#include "d3dx9.h"

#ifndef __D3DX9TEX_H__
#define __D3DX9TEX_H__


//----------------------------------------------------------------------------
// D3DX_FILTER flags:
// ------------------
//
// A valid filter must contain one of these values:
//
//  D3DX_FILTER_NONE
//      No scaling or filtering will take place.  Pixels outside the bounds
//      of the source image are assumed to be transparent black.
//  D3DX_FILTER_POINT
//      Each destination pixel is computed by sampling the nearest pixel
//      from the source image.
//  D3DX_FILTER_LINEAR
//      Each destination pixel is computed by linearly interpolating between
//      the nearest pixels in the source image.  This filter works best 
//      when the scale on each axis is less than 2.
//  D3DX_FILTER_TRIANGLE
//      Every pixel in the source image contributes equally to the
//      destination image.  This is the slowest of all the filters.
//  D3DX_FILTER_BOX
//      Each pixel is computed by averaging a 2x2(x2) box pixels from 
//      the source image. Only works when the dimensions of the 
//      destination are half those of the source. (as with mip maps)
//
// And can be OR'd with any of these optional flags:
//
//  D3DX_FILTER_MIRROR_U
//      Indicates that pixels off the edge of the texture on the U-axis
//      should be mirrored, not wraped.
//  D3DX_FILTER_MIRROR_V
//      Indicates that pixels off the edge of the texture on the V-axis
//      should be mirrored, not wraped.
//  D3DX_FILTER_MIRROR_W
//      Indicates that pixels off the edge of the texture on the W-axis
//      should be mirrored, not wraped.
//  D3DX_FILTER_MIRROR
//      Same as specifying D3DX_FILTER_MIRROR_U | D3DX_FILTER_MIRROR_V |
//      D3DX_FILTER_MIRROR_V
//  D3DX_FILTER_DITHER
//      Dithers the resulting image using a 4x4 order dither pattern.
//  D3DX_FILTER_SRGB_IN
//      Denotes that the input data is in sRGB (gamma 2.2) colorspace.
//  D3DX_FILTER_SRGB_OUT
//      Denotes that the output data is in sRGB (gamma 2.2) colorspace.
//  D3DX_FILTER_SRGB
//      Same as specifying D3DX_FILTER_SRGB_IN | D3DX_FILTER_SRGB_OUT
//
//----------------------------------------------------------------------------

#define D3DX_FILTER_NONE             (1 << 0)
#define D3DX_FILTER_POINT            (2 << 0)
#define D3DX_FILTER_LINEAR           (3 << 0)
#define D3DX_FILTER_TRIANGLE         (4 << 0)
#define D3DX_FILTER_BOX              (5 << 0)

#define D3DX_FILTER_MIRROR_U         (1 << 16)
#define D3DX_FILTER_MIRROR_V         (2 << 16)
#define D3DX_FILTER_MIRROR_W         (4 << 16)
#define D3DX_FILTER_MIRROR           (7 << 16)

#define D3DX_FILTER_DITHER           (1 << 19)
#define D3DX_FILTER_DITHER_DIFFUSION (2 << 19)

#define D3DX_FILTER_SRGB_IN          (1 << 21)
#define D3DX_FILTER_SRGB_OUT         (2 << 21)
#define D3DX_FILTER_SRGB             (3 << 21)


//-----------------------------------------------------------------------------
// D3DX_SKIP_DDS_MIP_LEVELS is used to skip mip levels when loading a DDS file:
//-----------------------------------------------------------------------------

#define D3DX_SKIP_DDS_MIP_LEVELS_MASK   0x1F
#define D3DX_SKIP_DDS_MIP_LEVELS_SHIFT  26
#define D3DX_SKIP_DDS_MIP_LEVELS(levels, filter) ((((levels) & D3DX_SKIP_DDS_MIP_LEVELS_MASK) << D3DX_SKIP_DDS_MIP_LEVELS_SHIFT) | ((filter) == D3DX_DEFAULT ? D3DX_FILTER_BOX : (filter)))




//----------------------------------------------------------------------------
// D3DX_NORMALMAP flags:
// ---------------------
// These flags are used to control how D3DXComputeNormalMap generates normal
// maps.  Any number of these flags may be OR'd together in any combination.
//
//  D3DX_NORMALMAP_MIRROR_U
//      Indicates that pixels off the edge of the texture on the U-axis
//      should be mirrored, not wraped.
//  D3DX_NORMALMAP_MIRROR_V
//      Indicates that pixels off the edge of the texture on the V-axis
//      should be mirrored, not wraped.
//  D3DX_NORMALMAP_MIRROR
//      Same as specifying D3DX_NORMALMAP_MIRROR_U | D3DX_NORMALMAP_MIRROR_V
//  D3DX_NORMALMAP_INVERTSIGN
//      Inverts the direction of each normal 
//  D3DX_NORMALMAP_COMPUTE_OCCLUSION
//      Compute the per pixel Occlusion term and encodes it into the alpha.
//      An Alpha of 1 means that the pixel is not obscured in anyway, and
//      an alpha of 0 would mean that the pixel is completly obscured.
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------

#define D3DX_NORMALMAP_MIRROR_U     (1 << 16)
#define D3DX_NORMALMAP_MIRROR_V     (2 << 16)
#define D3DX_NORMALMAP_MIRROR       (3 << 16)
#define D3DX_NORMALMAP_INVERTSIGN   (8 << 16)
#define D3DX_NORMALMAP_COMPUTE_OCCLUSION (16 << 16)




//----------------------------------------------------------------------------
// D3DX_CHANNEL flags:
// -------------------
// These flags are used by functions which operate on or more channels
// in a texture.
//
// D3DX_CHANNEL_RED
//     Indicates the red channel should be used
// D3DX_CHANNEL_BLUE
//     Indicates the blue channel should be used
// D3DX_CHANNEL_GREEN
//     Indicates the green channel should be used
// D3DX_CHANNEL_ALPHA
//     Indicates the alpha channel should be used
// D3DX_CHANNEL_LUMINANCE
//     Indicates the luminaces of the red green and blue channels should be 
//     used.
//
//----------------------------------------------------------------------------

#define D3DX_CHANNEL_RED            (1 << 0)
#define D3DX_CHANNEL_BLUE           (1 << 1)
#define D3DX_CHANNEL_GREEN          (1 << 2)
#define D3DX_CHANNEL_ALPHA          (1 << 3)
#define D3DX_CHANNEL_LUMINANCE      (1 << 4)




//----------------------------------------------------------------------------
// D3DXIMAGE_FILEFORMAT:
// ---------------------
// This enum is used to describe supported image file formats.
//
//----------------------------------------------------------------------------

typedef enum _D3DXIMAGE_FILEFORMAT
{
    D3DXIFF_BMP         = 0,
    D3DXIFF_JPG         = 1,
    D3DXIFF_TGA         = 2,
    D3DXIFF_PNG         = 3,
    D3DXIFF_DDS         = 4,
    D3DXIFF_PPM         = 5,
    D3DXIFF_DIB         = 6,
    D3DXIFF_HDR         = 7,       //high dynamic range formats
    D3DXIFF_PFM         = 8,       //
    D3DXIFF_FORCE_DWORD = 0x7fffffff

} D3DXIMAGE_FILEFORMAT;


#if 0
//----------------------------------------------------------------------------
// LPD3DXFILL2D and LPD3DXFILL3D:
// ------------------------------
// Function types used by the texture fill functions.
//
// Parameters:
//  pOut
//      Pointer to a vector which the function uses to return its result.
//      X,Y,Z,W will be mapped to R,G,B,A respectivly. 
//  pTexCoord
//      Pointer to a vector containing the coordinates of the texel currently 
//      being evaluated.  Textures and VolumeTexture texcoord components 
//      range from 0 to 1. CubeTexture texcoord component range from -1 to 1.
//  pTexelSize
//      Pointer to a vector containing the dimensions of the current texel.
//  pData
//      Pointer to user data.
//
//----------------------------------------------------------------------------

typedef VOID (WINAPI *LPD3DXFILL2D)(D3DXVECTOR4 *pOut, 
    CONST D3DXVECTOR2 *pTexCoord, CONST D3DXVECTOR2 *pTexelSize, LPVOID pData);

typedef VOID (WINAPI *LPD3DXFILL3D)(D3DXVECTOR4 *pOut, 
    CONST D3DXVECTOR3 *pTexCoord, CONST D3DXVECTOR3 *pTexelSize, LPVOID pData);
#endif
 


//----------------------------------------------------------------------------
// D3DXIMAGE_INFO:
// ---------------
// This structure is used to return a rough description of what the
// the original contents of an image file looked like.
// 
//  Width
//      Width of original image in pixels
//  Height
//      Height of original image in pixels
//  Depth
//      Depth of original image in pixels
//  MipLevels
//      Number of mip levels in original image
//  Format
//      D3D format which most closely describes the data in original image
//  ResourceType
//      D3DRESOURCETYPE representing the type of texture stored in the file.
//      D3DRTYPE_TEXTURE, D3DRTYPE_VOLUMETEXTURE, or D3DRTYPE_CUBETEXTURE.
//  ImageFileFormat
//      D3DXIMAGE_FILEFORMAT representing the format of the image file.
//
//----------------------------------------------------------------------------

typedef struct _D3DXIMAGE_INFO
{
    UINT                    Width;
    UINT                    Height;
    UINT                    Depth;
    UINT                    MipLevels;
    D3DFORMAT               Format;
    D3DRESOURCETYPE         ResourceType;
    D3DXIMAGE_FILEFORMAT    ImageFileFormat;

} D3DXIMAGE_INFO;



#ifdef __cplusplus
extern "C" {
#endif //__cplusplus


//----------------------------------------------------------------------------
// D3DXSaveSurfaceToFile:
// ----------------------
// Save a surface to a image file.
//
// Parameters:
//  pDestFile
//      File name of the destination file
//  DestFormat
//      D3DXIMAGE_FILEFORMAT specifying file format to use when saving.
//  pSrcSurface
//      Source surface, containing the image to be saved
//  pSrcPalette
//      Source palette of 256 colors, or NULL
//  pSrcRect
//      Source rectangle, or NULL for the entire image
//
//----------------------------------------------------------------------------

HRESULT WINAPI
    D3DXSaveSurfaceToFileA(
        LPCSTR                    pDestFile,
        D3DXIMAGE_FILEFORMAT      DestFormat,
        LPDIRECT3DSURFACE9        pSrcSurface,
        CONST PALETTEENTRY*       pSrcPalette,
        CONST RECT*               pSrcRect);

HRESULT WINAPI
    D3DXSaveSurfaceToFileW(
        LPCWSTR                   pDestFile,
        D3DXIMAGE_FILEFORMAT      DestFormat,
        LPDIRECT3DSURFACE9        pSrcSurface,
        CONST PALETTEENTRY*       pSrcPalette,
        CONST RECT*               pSrcRect);

#ifdef UNICODE
#define D3DXSaveSurfaceToFile D3DXSaveSurfaceToFileW
#else
#define D3DXSaveSurfaceToFile D3DXSaveSurfaceToFileA
#endif

#ifdef __cplusplus
}
#endif //__cplusplus


#endif //__D3DX9TEX_H__
