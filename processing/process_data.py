#!/usr/bin/env python3

import cv2
import numpy as np
from numpy import uint32, float32, sqrt
from os.path import abspath

import pycuda.autoinit
import pycuda.driver as drv
from pycuda.compiler import SourceModule

import h5py


def read_depth_img(name):
    return cv2.flip(cv2.imread('../depth_upsample_data/{}'.format(name),
                               cv2.IMREAD_UNCHANGED) / (2 ** 16), 0)


def read_color_img(name):
    return cv2.flip(cv2.imread('../depth_upsample_data/{}'.format(name),
                               cv2.IMREAD_COLOR) / (2 ** 8), 0)


def show_image(img, label='Depth Image', wait_period=0):
    cv2.imshow(label, cv2.resize(img, (1600, 900),
                                 interpolation=cv2.INTER_NEAREST))
    cv2.waitKey(wait_period)


def downsample(img):
    return cv2.resize(img, None, fx=0.5,
                      fy=0.5, interpolation=cv2.INTER_NEAREST)


def generate_image_data(image, high_res_image,
                        patches_array, results_array, func):
    counter = np.zeros(1, dtype=uint32)
    counter2 = np.zeros(1, dtype=uint32)
    bs = 32
    resolution = image.shape
    func(drv.InOut(image),
         drv.InOut(high_res_image),
         uint32(image.shape[0]),
         uint32(image.shape[1]),
         drv.InOut(patches_array),
         drv.InOut(results_array),
         uint32(patches_array.shape[0]),
         drv.InOut(counter),
         drv.InOut(counter2),
         block=(bs, bs, 1),
         grid=(resolution[0] // bs, resolution[1] // bs, 1))
    print(counter)
    print(counter2)
    '''for k in range(0, 30000):
        ind = hash_table[k // 2][k % 2][1]
        if ind == 0:
            continue
        print(hash_table[k // 2])
        print("Checking index: {}".format(ind))
        print(patches_array[ind, :, :, 0])
        patch = patches_array[ind, :, :, :3]
        high_res = results_array[ind, :, :, :3]
        show_image(patch, 'Depth Image', 0)
        show_image(high_res, 'Depth Image', 0)'''
    return counter[0]


def write_results_to_file(patches_np, results_np):
    with h5py.File("output.hdf5", "w") as f:
        len_samples = patches_np.shape[0]
        training_samples = int(0.9 * len_samples)
        train_np = (patches_np[:training_samples],
                    results_np[:training_samples])
        test_np = (patches_np[training_samples:],
                   results_np[training_samples:])
        train_dset = f.create_group('train')
        test_dset = f.create_group('test')
        train_dset.create_dataset('features', data=train_np[0])
        train_dset.create_dataset('predictions', data=train_np[1])
        test_dset.create_dataset('features', data=test_np[0])
        test_dset.create_dataset('predictions', data=test_np[1])


if __name__ == '__main__':
    all_patches = np.zeros((2700000, 7, 7, 4), dtype=np.float32)
    all_results = np.zeros((2700000, 2, 2, 3), dtype=np.float32)

    patches_np = np.zeros((2000000, 7, 7, 4), dtype=np.float32)
    results_np = np.zeros((2000000, 2, 2, 3), dtype=np.float32)

    with open('patches.cu', 'r') as f:
        mod = SourceModule(f.read(), include_dirs=[
            abspath('./')],
            no_extern_c=True)

    num_results = 0
    #for i in range(210, 1320, 30):
    for i in range(30, 300, 30):
        img_depth = read_depth_img('{}_depth.pgm'.format(i))
        img_color = read_color_img('{}_color.png'.format(i)).astype(float32)
        downsampled_depth = downsample(img_depth)
        downsampled_color = downsample(img_color).astype(float32)
        downsampled_depth = \
            np.reshape(downsampled_depth,
                       (downsampled_depth.shape[0],
                        downsampled_depth.shape[1], 1)) ** 32
        downsampled_combined = np.append(
            downsampled_color, downsampled_depth, axis=2).astype(float32)

        #show_image(downsampled_combined[:, :, 3])
        show_image(img_depth ** 30, 'Image', 0)
        show_image(img_color, 'Image', 0)
        end = generate_image_data(downsampled_combined,
                                  img_color,
                                  patches_np,
                                  results_np,
                                  mod.get_function('image_hash'))

        if num_results + end >= all_patches.shape[0]:
            end = all_patches.shape[0] - num_results

        all_patches[num_results:num_results + end] = patches_np[:end]
        all_results[num_results:num_results + end] = results_np[:end]

        patches_np.fill(0)
        results_np.fill(0)

        num_results += end

        print("Num samples: {}".format(num_results))

        if num_results == all_patches.shape[0]:
            break

    write_results_to_file(all_patches[:num_results],
                          all_results[:num_results])
