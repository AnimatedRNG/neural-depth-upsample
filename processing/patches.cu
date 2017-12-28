#include <stdio.h>
#include <math.h>
#include "helper_math.h"

__device__ float rand(float x, float y){
    //return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
    float a = sin(x * 12.9898 + y * 78.233) * 43758.5453;
    return a - floor(a);
}

extern "C"
__global__ void image_hash(
    float* image,
    float* high_res_image,
    unsigned int width,
    unsigned int height,
    float* patches,
    float* results,
    unsigned int patches_length,
    unsigned int* atomic_counter) {
    const uint center_i = threadIdx.x + blockIdx.x * blockDim.x;
    const uint center_j = threadIdx.y + blockIdx.y * blockDim.y;
    const uint center_k = threadIdx.z + blockIdx.z * blockDim.z;
    
    const uint global_id = center_i * height + center_j;
    
    float patch[7][7][4];
    float high_res[2][2][3];
    unsigned int num_nonzero = 0;

    // Very inefficient
    // TODO: Move brightness scaling and color channel swap to a python
    // class that acts like keras ImageDataGenerator
    int ordering[3];
    int start = global_id % 3;
    int a = 0;
    for (int i = start; i < start + 3; i++) {
        ordering[a++] = i % 3;
    }

    float brightness = 1.0 + rand((float) center_i, (float) center_j) * 0.2;
    
    for (int offset_i = 0; offset_i < 2; offset_i++) {
        for (int offset_j = 0; offset_j < 2; offset_j++) {
            int i = (center_i * 2) + offset_i;
            int j = (center_j * 2) + offset_j;
            int ind = 3 * (i * (height * 2) + j);
            
            high_res[offset_i][offset_j][0] =
                high_res_image[ind + ordering[0]] * brightness;
            high_res[offset_i][offset_j][1] =
                high_res_image[ind + ordering[1]] * brightness;
            high_res[offset_i][offset_j][2] =
                high_res_image[ind + ordering[2]] * brightness;
        }
    }
    
    for (int offset_i = -3; offset_i <= 3; offset_i++) {
        for (int offset_j = -3; offset_j <= 3; offset_j++) {
            int i = offset_i + center_i;
            int j = offset_j + center_j;
            
            int norm_offset_i = offset_i + 3;
            int norm_offset_j = offset_j + 3;
            
            float r = 0.0;
            float g = 0.0;
            float b = 0.0;
            float d = 0.0;
            int image_index = 0;
            if (i >= 0 && j >= 0 && i < width && i < height) {
                image_index = 4 * (i * height + j);
                r = image[image_index + ordering[0]] * brightness;
                g = image[image_index + ordering[1]] * brightness;
                b = image[image_index + ordering[2]] * brightness;
                d = image[image_index + 3];
                num_nonzero++;
            }
            
            patch[norm_offset_i][norm_offset_j][0] = r;
            patch[norm_offset_i][norm_offset_j][1] = g;
            patch[norm_offset_i][norm_offset_j][2] = b;
            patch[norm_offset_i][norm_offset_j][3] = d;
        }
    }

    unsigned int my_patch_id = atomicAdd(atomic_counter, 1);
    for (int offset_i = -3; offset_i <= 3; offset_i++) {
        for (int offset_j = -3; offset_j <= 3; offset_j++) {
            int norm_offset_i = offset_i + 3;
            int norm_offset_j = offset_j + 3;
            unsigned int patches_index = my_patch_id * (7 * 7 * 4) +
                                         norm_offset_i * (7 * 4) + norm_offset_j * 4;

            patches[patches_index] = patch[norm_offset_i][norm_offset_j][0];
            patches[patches_index + 1] = patch[norm_offset_i][norm_offset_j][1];
            patches[patches_index + 2] = patch[norm_offset_i][norm_offset_j][2];
            patches[patches_index + 3] = patch[norm_offset_i][norm_offset_j][3];
        }
    }

    for (int offset_i = 0; offset_i < 2; offset_i++) {
        for (int offset_j = 0; offset_j < 2; offset_j++) {
            unsigned int results_index =
                my_patch_id * (2 * 2 * 3) + offset_i * (2 * 3) + offset_j * 3;
            results[results_index] = high_res[offset_i][offset_j][0];
            results[results_index + 1] = high_res[offset_i][offset_j][1];
            results[results_index + 2] = high_res[offset_i][offset_j][2];
        }
    }
}
