//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#include <metal_stdlib>

using namespace metal;

struct IA2VS {
    float3 position [[ attribute(0) ]];
    ushort4 joints  [[ attribute(1) ]];
    float4 weights  [[ attribute(2) ]];
};

struct VS2RS {
    float4 position [[ position ]];
};

vertex VS2RS vs_main(const    IA2VS     input             [[ stage_in  ]],
                     constant float4x4& projection_matrix [[ buffer(0) ]],
                     constant float4x4& view_matrix       [[ buffer(1) ]],
                     constant float4x4& model_matrix      [[ buffer(2) ]],
                     constant float4x4* joint_palette     [[ buffer(3) ]]) {
    VS2RS output;

    float4x4 skin_matrix = input.weights.x * joint_palette[input.joints.x] +
                           input.weights.y * joint_palette[input.joints.y] +
                           input.weights.z * joint_palette[input.joints.z] +
                           input.weights.w * joint_palette[input.joints.w];

    output.position = projection_matrix *
                      view_matrix *
                      model_matrix *
                      skin_matrix *
                      float4(input.position, 1.0);

    return output;
}

fragment float4 fs_main() {
    return float4(1.0, 1.0, 1.0, 1.0);
}
