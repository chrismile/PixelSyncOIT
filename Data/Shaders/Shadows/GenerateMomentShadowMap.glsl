#version 120

out float fragDepth;

void gatherFragment(vec4 color)
{
	float depth = logDepthWarp(-screenSpacePosition.z, logDepthMinShadow, logDepthMaxShadow);
    float opacity = color.a;
}
