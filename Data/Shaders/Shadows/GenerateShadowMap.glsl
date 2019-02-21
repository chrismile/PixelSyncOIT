in vec4 gl_FragCoord;
out float fragDepth;

void gatherFragment(vec4 color)
{
#ifdef MODEL_WITH_VORTICITY
    // Testing: No shadows from fragments with low vorticity
    if (color.a < 0.36629214882850647) {
        discard;
    }
#endif
    fragDepth = gl_FragCoord.z;
}
