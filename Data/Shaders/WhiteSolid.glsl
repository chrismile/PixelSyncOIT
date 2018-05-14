-- Vertex

uniform mat4 mvpMatrix;
attribute vec4 position;

void main()
{
	gl_Position = mvpMatrix * position;
}

-- Fragment

uniform sampler2D texture;

void main()
{
	gl_FragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}
