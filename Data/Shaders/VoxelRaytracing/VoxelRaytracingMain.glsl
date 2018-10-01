-- Compute

#version 430

layout (local_size_x = 8, local_size_y = 4, local_size_z = 1) in;

struct LineSegment
{
	vec3 v1; // Vertex position
	float a1; // Vertex attribute
	vec3 v2; // Vertex position
	float a2; // Vertex attribute
};

// Buffer containing all line segments
layout (std430, binding = 0) writeonly buffer LineSegmentBuffer
{
	LineSegment lineSegments[];
};

// Offset of voxels in buffer above
layout (std430, binding = 1) readonly buffer VoxelLineListOffsetBuffer
{
	uint voxelLineListOffsets[];
};

// Density of voxels with mip-map LODs
uniform sampler3D densityTexture;

// Output of rasterizer
layout(rgba32f, binding = 0) uniform image2D imageOutput;


// Size of the rendering viewport (/window)
uniform ivec2 viewportSize;

// Convert lines in voxel space to clip space
uniform mat4 voxelToClipSpace;


void main()
{
	uvec2 globalID = gl_GlobalInvocationID.xy;
	if (globalID.x < surfaceSize.x || globalID.x < surfaceSize.y) {
		// Outside of viewport
		return;
	}

	ivec2 fragCoord = ivec2(globalID);
	vec4 fragColor = vec4(0.0);

	// Testing code
	if (length(vec2(fragCoord)) < 20.0f) {
		fragColor = vec4(0.0, 1.0, 0.0, 1.0);
	}

	imageStore(imageOutput, fragCoord, fragColor);
}
