
//#define ADDRESSING_TILED_2x2
//#define ADDRESSING_TILED_2x8
//#define ADDRESSING_TILED_NxM
//#define TILE_N 2u
//#define TILE_M 8u

// Address 1D structured buffers as tiled to better data exploit locality
// "OIT to Volumetric Shadow Mapping, 101 Uses for Raster Ordered Views using DirectX 12",
// by Leigh Davies (Intel), March 05, 2015
uint addrGen(uvec2 addr2D)
{
#if defined(ADDRESSING_TILED_2x2)
	uint surfaceWidth = viewportW >> 1U; // / 2U
	uvec2 tileAddr2D = addr2D >> 1U; // / 2U
	uint tileAddr1D = (tileAddr2D.x + surfaceWidth * tileAddr2D.y) << 2U; // * 4U
	uvec2 pixelAddr2D = addr2D & 0x1U;
	uint pixelAddr1D = (pixelAddr2D.x) + (pixelAddr2D.y << 1U);
	return tileAddr1D | pixelAddr1D;
#elif defined(ADDRESSING_TILED_2x8)
	uint surfaceWidth = viewportW >> 1U; // / 2U;
	uvec2 tileAddr2D = addr2D / uvec2(2U, 8U);
	uint tileAddr1D = (tileAddr2D.x + surfaceWidth * tileAddr2D.y) << 4U; // * 16U;
	uvec2 pixelAddr2D = addr2D & uvec2(1U, 7U);
	uint pixelAddr1D = pixelAddr2D.x + pixelAddr2D.y * 2U;
	return tileAddr1D | pixelAddr1D;
#elif defined(ADDRESSING_TILED_NxM)
	uint surfaceWidth = viewportW / TILE_N;
	uvec2 tileAddr2D = addr2D / uvec2(TILE_N, TILE_M);
	uint tileAddr1D = (tileAddr2D.x + surfaceWidth * tileAddr2D.y) * (TILE_N * TILE_M);
	uvec2 pixelAddr2D = addr2D & uvec2(TILE_N-1, TILE_M-1);
	uint pixelAddr1D = pixelAddr2D.x + pixelAddr2D.y * TILE_N;
	return tileAddr1D | pixelAddr1D;
#else
	return addr2D.x + viewportW * addr2D.y;
#endif
}

// For use with Image Load/Store
ivec2 addrGen2D(ivec2 addr2D)
{
    int addr1D = int(addrGen(addr2D));
    return ivec2(addr1D%viewportW, addr1D/viewportW);
}