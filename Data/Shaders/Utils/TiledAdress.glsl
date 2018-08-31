#define ADDRESSING_TILED

// Address 1D structured buffers as tiled to better data exploit locality
// "OIT to Volumetric Shadow Mapping, 101 Uses for Raster Ordered Views using DirectX 12",
// by Leigh Davies (Intel), March 05, 2015
uint addrGen(uvec2 addr2D)
{
#ifdef ADDRESSING_TILED
	uint surfaceWidth = viewportW >> 1U;
	uvec2 tileAddr2D = addr2D >> 1U;
	uint tileAddr1D = (tileAddr2D.x + surfaceWidth * tileAddr2D.y) << 2U;
	uvec2 pixelAddr2D = addr2D & 0x1U;
	uint pixelAddr1D = (pixelAddr2D.x << 1U) + pixelAddr2D.y;
	return tileAddr1D | pixelAddr1D;
#else
	return addr2D.x + viewportW * addr2D.y;
#endif
}