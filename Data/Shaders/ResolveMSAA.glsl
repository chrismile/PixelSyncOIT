-- Vertex

uniform mat4 mvpMatrix;
attribute vec4 position;
attribute vec2 texcoord;
varying vec2 fragTexCoord;

void main()
{
	fragTexCoord = texcoord;
	gl_Position = mvpMatrix * position;
}

-- Fragment

uniform sampler2DMS texture;
uniform int numSamples;
varying vec2 fragTexCoord;
 
void main()
{
	ivec2 size = textureSize(texture);
	ivec2 iCoords = ivec2(int(fragTexCoord.x*size.x), int(fragTexCoord.y*size.y));
	vec3 color = vec3(0, 0, 0); 
	for (int sample = 0; sample < numSamples; sample++) {
		color += texelFetch(texture, iCoords, sample).rgb;
	}
	gl_FragColor = vec4(color / numSamples, 1);
}
