-- Vertex.Plain

uniform mat4 mvpMatrix;
attribute vec4 vertexPosition;

void main()
{
	gl_Position = mvpMatrix * vertexPosition;
}

-- Fragment.Plain

uniform vec4 color;

void main()
{
	gl_FragColor = color;
}



-- Vertex.Textured

uniform mat4 mvpMatrix;
attribute vec4 vertexPosition;
attribute vec2 texcoord;
varying vec2 fragTexCoord;

void main()
{
	fragTexCoord = texcoord;
	gl_Position = mvpMatrix * vertexPosition;
}

-- Fragment.Textured

uniform sampler2D texture;
uniform vec4 color;
varying vec2 fragTexCoord;

void main()
{
	gl_FragColor = color * texture2D(texture, fragTexCoord);
}
