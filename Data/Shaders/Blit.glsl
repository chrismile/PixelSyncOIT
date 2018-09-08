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

uniform sampler2D texture;
varying vec2 fragTexCoord;

void main()
{
	gl_FragColor = texture2D(texture, fragTexCoord);
}
