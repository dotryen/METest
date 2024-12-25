struct FragInput {
    float4 position : SV_POSITION;
};

float4 fragment(FragInput input) : SV_TARGET {
    return float4(0.8, 0.8, 0.8, 1.0);
}
