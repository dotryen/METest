struct WorldBuffer {
    column_major float4x4 view;
    column_major float4x4 proj;
};

struct ObjectBuffer {
    column_major float4x4 transform;
};

ConstantBuffer<WorldBuffer> world : register(b0, space1);
ConstantBuffer<ObjectBuffer> object : register(b1, space1);

struct VertInput {
    float3 position : POSITION;
};

struct VertOutput {
    float4 position : SV_POSITION;
};

VertOutput vertex(VertInput input) {
    VertOutput output;
    output.position = mul(world.proj, mul(world.view, mul(object.transform, float4(input.position, -1.0))));
    return output;
}
