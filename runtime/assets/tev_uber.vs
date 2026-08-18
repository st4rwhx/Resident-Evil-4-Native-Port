#version 330 core

in vec3 vertexPosition;
in vec4 vertexColor;
in vec2 vertexTexCoord0;
in vec2 vertexTexCoord1;
in vec2 vertexTexCoord2;
in vec2 vertexTexCoord3;
in vec2 vertexTexCoord4;
in vec2 vertexTexCoord5;
in vec2 vertexTexCoord6;
in vec2 vertexTexCoord7;

out vec4 fragColor;
out vec2 fragTexCoord[8];

uniform mat4 mvp;

void main() {
    fragColor = vertexColor;
    fragTexCoord[0] = vertexTexCoord0;
    fragTexCoord[1] = vertexTexCoord1;
    fragTexCoord[2] = vertexTexCoord2;
    fragTexCoord[3] = vertexTexCoord3;
    fragTexCoord[4] = vertexTexCoord4;
    fragTexCoord[5] = vertexTexCoord5;
    fragTexCoord[6] = vertexTexCoord6;
    fragTexCoord[7] = vertexTexCoord7;

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
