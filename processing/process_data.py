#!/usr/bin/env python3

import cv2
import numpy as np
from numpy import uint32, float32, sqrt
from os.path import abspath
from os import listdir
from sys import argv

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
    bs = 16
    resolution = image.shape
    func(drv.InOut(image),
         drv.InOut(high_res_image),
         uint32(image.shape[0]),
         uint32(image.shape[1]),
         drv.InOut(patches_array),
         drv.InOut(results_array),
         uint32(patches_array.shape[0]),
         drv.InOut(counter),
         block=(bs, bs, 1),
         grid=(resolution[0] // bs, resolution[1] // bs, 1))
    print(counter)
    return counter[0]


def create_results_file(size_in_bytes=10000000000, chunk_size=1000):
    num_elements = size_in_bytes // (7 * 7 * 4)
    with h5py.File("output.hdf5", "w") as f:
        train_dset = f.create_group('train')
        test_dset = f.create_group('test')

        train_dset.create_dataset(
            "features", (num_elements, 7, 7, 4),
            chunks=(chunk_size, 7, 7, 4), dtype='f4')
        train_dset.create_dataset(
            "predictions", (num_elements, 2, 2, 3),
            chunks=(chunk_size, 2, 2, 3), dtype='f4')

        test_dset.create_dataset(
            "features", (num_elements, 7, 7, 4),
            chunks=(chunk_size, 7, 7, 4), dtype='f4')
        test_dset.create_dataset(
            "predictions", (num_elements, 2, 2, 3),
            chunks=(chunk_size, 2, 2, 3), dtype='f4')


def append_results_to_file(patches_np, results_np, current_end_train, current_end_test):
    with h5py.File("output.hdf5", "r+") as f:
        len_samples = patches_np.shape[0]
        training_samples = int(0.9 * len_samples)
        testing_samples = len_samples - int(0.9 * len_samples)
        f['train']['features'][current_end_train:current_end_train + training_samples] = \
            patches_np[:training_samples]
        f['train']['predictions'][current_end_train:current_end_train + training_samples] = \
            results_np[:training_samples]
        f['test']['features'][current_end_test:current_end_test + testing_samples] = \
            patches_np[training_samples:]
        f['test']['predictions'][current_end_test:current_end_test + testing_samples] = \
            results_np[training_samples:]
    return (training_samples + len_samples, testing_samples + len_samples)


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


def truncate_file(current_end_training, current_end_testing):
    with h5py.File("output.hdf5", "r+") as f:
        f['train']['features'].resize((current_end_training, 7, 7, 4))
        f['train']['predictions'].resize((current_end_training, 2, 2, 3))

        f['test']['features'].resize((current_end_testing, 7, 7, 4))
        f['test']['predictions'].resize((current_end_testing, 2, 2, 3))


def get_image_filenumbers_in_dir(directory):
    file_set = set(listdir(directory))
    file_names = []
    for filename in listdir(directory):
        if filename.endswith("_color.png"):
            number = filename[:-10]
            print("File number: {}".format(number))
            if number + "_depth.pgm" in file_set:
                file_names.append(number)
    return file_names


if __name__ == '__main__':
    assert len(argv) == 2

    patches_np = np.zeros((2000000, 7, 7, 4), dtype=np.float32)
    results_np = np.zeros((2000000, 2, 2, 3), dtype=np.float32)

    with open('patches.cu', 'r') as f:
        mod = SourceModule(f.read(), include_dirs=[
            abspath('./')],
            no_extern_c=True)

    create_results_file()

    num_results_training, num_results_testing = 0, 0
    # for i in range(210, 1320, 30):
    for i in get_image_filenumbers_in_dir(argv[1]):
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

        # show_image(downsampled_combined[:, :, 3])
        show_image(img_depth ** 30, 'Image', 0)
        show_image(img_color, 'Image', 0)
        end = generate_image_data(downsampled_combined,
                                  img_color,
                                  patches_np,
                                  results_np,
                                  mod.get_function('image_hash'))

        num_results_training, num_results_testing = \
            append_results_to_file(patches_np[:end], results_np[:end],
                                   num_results_training, num_results_testing)

        patches_np.fill(0)
        results_np.fill(0)

        print("Num samples (training): {}".format(num_results_training))
        print("Num samples (testing): {}".format(num_results_testing))

        if (num_results_training >= 12000000):
            break
    truncate_file(num_results_training, num_results_testing)
