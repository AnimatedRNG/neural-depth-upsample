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
    cv2.imshow(label, cv2.resize(img, (3200, 1800),
                                 interpolation=cv2.INTER_NEAREST))
    cv2.waitKey(wait_period)


def downsample(img):
    return cv2.resize(img, None, fx=0.5,
                      fy=0.5, interpolation=cv2.INTER_NEAREST)


def generate_image_data(image, high_res_image, hash_table,
                        patches_array, results_array, func):
    counter = np.zeros(1, dtype=uint32)
    counter2 = np.zeros(1, dtype=uint32)
    bs = 32
    resolution = image.shape
    func(drv.InOut(image),
         drv.InOut(high_res_image),
         uint32(image.shape[0]),
         uint32(image.shape[1]),
         drv.InOut(hash_table),
         uint32(hash_table.shape[0]),
         uint32(hash_table.shape[1]),
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
        training_samples = int(0.7 * len_samples)
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
    img_depth = read_depth_img('270_depth.pgm')
    img_color = read_color_img('270_color.png').astype(float32)
    downsampled_depth = downsample(img_depth)
    downsampled_color = downsample(img_color).astype(float32)
    downsampled_depth = \
        np.reshape(downsampled_depth,
                   (downsampled_depth.shape[0], downsampled_depth.shape[1],
                    1)) ** 32
    downsampled_combined = np.append(
        downsampled_color, downsampled_depth, axis=2).astype(float32)

    # Entries are (hash_value, id)
    hash_table_np = np.zeros((5000000, 2, 2), dtype=np.uint64)
    patches_np = np.zeros((2000000, 7, 7, 4), dtype=np.float32)
    results_np = np.zeros((2000000, 2, 2, 3), dtype=np.float32)

    with open('fast_hash.cu', 'r') as f:
        mod = SourceModule(f.read(), include_dirs=[
            abspath('./')],
            no_extern_c=True)
    #show_image(downsampled_combined[:, :, 3])
    show_image(img_depth ** 30)
    show_image(img_color)
    end = generate_image_data(downsampled_combined,
                              img_color,
                              hash_table_np,
                              patches_np,
                              results_np,
                              mod.get_function('image_hash'))
    write_results_to_file(patches_np[:end], results_np[:end])
