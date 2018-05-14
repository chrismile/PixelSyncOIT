-- Vertex

uniform mat4 mvpMatrix;
uniform vec4 color;

attribute vec3 vertexPosition;
attribute vec3 vertexNormal;
varying vec3 normal;

void main()
{
	normal = vertexNormal;
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}

-- Fragment

uniform vec4 color;
varying vec3 normal;

void main()
{
	gl_FragColor = vec4(color.rgb * (dot(normal, vec3(1.0,0.0,0.0))/4.0+0.75), color.a);
	//gl_FragColor = color + vec4(normal, 1.0) + vec4(0.1,0.1,0.1,1.0);
}
