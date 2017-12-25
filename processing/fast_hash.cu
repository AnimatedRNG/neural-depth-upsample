#include "stdio.h"
#include "helper_math.h"

extern "C"
__global__ void image_hash(
    float* image,
    float* high_res_image,
    unsigned int width,
    unsigned int height,
    unsigned long long* hash_table,
    unsigned int hash_table_length,
    unsigned int hash_table_width,
    float* patches,
    float* results,
    unsigned int patches_length, unsigned int* atomic_counter) {
    const uint center_i = threadIdx.x + blockIdx.x * blockDim.x;
    const uint center_j = threadIdx.y + blockIdx.y * blockDim.y;
    const uint center_k = threadIdx.z + blockIdx.z * blockDim.z;
    
    const uint global_id = center_i * height + center_j;
    
    float patch[7][7][4];
    float high_res[2][2][3];
    float luma[7][7];
    float average = 0.0;
    unsigned int num_nonzero = 0;

    for (int offset_i = 0; offset_i < 2; offset_i++) {
        for (int offset_j = 0; offset_j < 2; offset_j++) {
            int i = (center_i * 2) + offset_i;
            int j = (center_j * 2) + offset_j;
            int ind = 3 * (i * (height * 2) + j);
            
            high_res[offset_i][offset_j][0] =
                high_res_image[ind];
            high_res[offset_i][offset_j][1] =
                high_res_image[ind + 1];
            high_res[offset_i][offset_j][2] =
                high_res_image[ind + 2];
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
                r = image[image_index];
                g = image[image_index + 1];
                b = image[image_index + 2];
                d = image[image_index + 3];
                num_nonzero++;
            }
            
            patch[norm_offset_i][norm_offset_j][0] = r;
            patch[norm_offset_i][norm_offset_j][1] = g;
            patch[norm_offset_i][norm_offset_j][2] = b;
            patch[norm_offset_i][norm_offset_j][3] = d;
            
            float this_luma = sqrt(0.2061 * r * r +
                                   0.5283 * g * g +
                                   0.1026 * b * b +
                                   0.1 * d * d);
                                   
            luma[norm_offset_i][norm_offset_j] = this_luma;
            average += this_luma;
        }
    }
    
    average /= num_nonzero;
    
    unsigned long long hash_value = 0;
    int current_index = 0;
    for (int offset_i = -3; offset_i <= 3; offset_i++) {
        for (int offset_j = -3; offset_j <= 3; offset_j++) {
            int norm_offset_i = offset_i + 3;
            int norm_offset_j = offset_j + 3;
            
            float this_luma = luma[norm_offset_i][norm_offset_j];
            if (this_luma > average) {
                hash_value |= 1ULL << current_index;
            }
            current_index++;
        }
    }
    
    unsigned int hash_base_index = (hash_value % hash_table_length) *
                                   hash_table_width * 2;
    for (int a = 0; a < hash_table_width * 2; a += 2) {
        unsigned int hash_index = hash_base_index + a;
        
        // Only replaces if there wasn't anything already there...
        // potentially inefficient
        if (atomicCAS(&(hash_table[hash_index]), 0, hash_value) == 0) {
            unsigned int my_patch_id = atomicAdd(atomic_counter, 1);
            hash_table[hash_index + 1] = my_patch_id;
            
            for (int offset_i = -3; offset_i <= 3; offset_i++) {
                for (int offset_j = -3; offset_j <= 3; offset_j++) {
                    int norm_offset_i = offset_i + 3;
                    int norm_offset_j = offset_j + 3;
                    unsigned int patches_index = my_patch_id * (7 * 7 * 4) +
                                                 norm_offset_i * (7 * 4) + norm_offset_j * 4;
                    
                    if (patches_index < 20000000) {
                        patches[patches_index] = patch[norm_offset_i][norm_offset_j][0];
                        patches[patches_index + 1] = patch[norm_offset_i][norm_offset_j][1];
                        patches[patches_index + 2] = patch[norm_offset_i][norm_offset_j][2];
                        patches[patches_index + 3] = patch[norm_offset_i][norm_offset_j][3];
                    }
                }
            }

            for (int offset_i = 0; offset_i < 2; offset_i++) {
                for (int offset_j = 0; offset_j < 2; offset_j++) {
                    unsigned int results_index =
                        my_patch_id * (2 * 2 * 3) + offset_i * (2 * 3) + offset_j * 3;
                    if (results_index < 20000000) {
                        results[results_index] = high_res[offset_i][offset_j][0];
                        results[results_index + 1] = high_res[offset_i][offset_j][1];
                        results[results_index + 2] = high_res[offset_i][offset_j][2];
                    }
                }
            }
            
            break;
        }
    }
    
    image[4 * global_id] = luma[3][3];
    image[4 * global_id + 1] = luma[3][3];
    image[4 * global_id + 2] = luma[3][3];
    image[4 * global_id + 3] = luma[3][3];
}
