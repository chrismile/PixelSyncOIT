-- Vertex.Plain

#version 430 core

in vec4 vertexPosition;

void main()
{
	gl_Position = mvpMatrix * vertexPosition;
}

-- Fragment.Plain

#version 430 core

uniform vec4 color;

void main()
{
	gl_FragColor = color;
}



-- Vertex.Textured

#version 430 core

in vec4 vertexPosition;
in vec2 texcoord;
out vec2 fragTexCoord;

void main()
{
	fragTexCoord = texcoord;
	gl_Position = mvpMatrix * vertexPosition;
}

-- Fragment.Textured

#version 430 core

uniform sampler2D texture;
uniform vec4 color;
in vec2 fragTexCoord;

void main()
{
	gl_FragColor = color * texture2D(texture, fragTexCoord);
}
