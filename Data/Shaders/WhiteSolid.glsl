-- Vertex

#version 430 core

in vec4 vertexPosition;

void main()
{
	gl_Position = mvpMatrix * vertexPosition;
}

-- Fragment

#version 430 core

uniform sampler2D texture;

void main()
{
	gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
