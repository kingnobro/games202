class PRTMaterial extends Material {

    constructor(precomputeL, vertexShader, fragmentShader) {
        // 需要将 precomputeL 拆分成 RGB, 以满足 glsl 数据的存储格式
        super(
        {
            'uPrecomputeL_R': {type: 'matrix3fv', value: precomputeL[0]},
            'uPrecomputeL_G': {type: 'matrix3fv', value: precomputeL[1]},
            'uPrecomputeL_B': {type: 'matrix3fv', value: precomputeL[2]},
        },
        [
            'aPrecomputeLT',
        ],
        vertexShader, fragmentShader, null);
    }
}

async function buildPRTMaterial(precomputeL, vertexPath, fragmentPath) {
    let vertexShader = await getShaderString(vertexPath);
    let fragmentShader = await getShaderString(fragmentPath);

    return new PRTMaterial(precomputeL, vertexShader, fragmentShader);
}