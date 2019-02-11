-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;
#ifdef MODEL_WITH_VORTICITY
layout(location = 2) in float vertexVorticity;
#endif

out float fragVorticity;

void main()
{
#ifdef MODEL_WITH_VORTICITY
    fragVorticity = vertexVorticity;
#endif
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#ifdef MODEL_WITH_VORTICITY
in float fragVorticity;
#endif
in vec4 gl_FragCoord;

out float fragDepth;

void main()
{
#ifdef MODEL_WITH_VORTICITY
    // Testing: No shadows from fragments with low vorticity
    if (fragVorticity < 0.36629214882850647) {
        discard;
    }
#endif
    fragDepth = gl_FragCoord.z;
}
