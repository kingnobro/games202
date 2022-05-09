attribute mat3 aPrecomputeLT;
attribute vec3 aVertexPosition;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
uniform mat3 uPrecomputeL_R;
uniform mat3 uPrecomputeL_G;
uniform mat3 uPrecomputeL_B;

varying highp vec3 vColor;

void main() {
    gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aVertexPosition, 1.0);

    vec3 Color = vec3(0.0, 0.0, 0.0);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            vec3 light = vec3(uPrecomputeL_R[i][j], uPrecomputeL_G[i][j], uPrecomputeL_B[i][j]);
            Color += light * aPrecomputeLT[i][j];
        }
    }
    vColor = Color;
}