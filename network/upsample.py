#!/usr/bin/env python3

import cv2
import numpy as np
from numpy import uint32, float32, sqrt, stack
from sys import argv, exit
from os.path import isfile
from random import shuffle

import h5py

import pandas
from keras.models import Sequential, load_model
from keras.layers import Dense, Dropout, Activation, Flatten, Reshape
from keras.layers import Conv2D, MaxPooling2D
from keras.layers import GaussianNoise, GaussianDropout
from keras.optimizers import Adam
from keras.wrappers.scikit_learn import KerasRegressor
from sklearn.model_selection import cross_val_score
from sklearn.model_selection import KFold
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline


def read_depth_img(name):
    return cv2.flip(cv2.imread(name,
                               cv2.IMREAD_UNCHANGED) / (2 ** 16), 0)


def read_color_img(name):
    return cv2.flip(cv2.imread(name,
                               cv2.IMREAD_COLOR) / (2 ** 8), 0)


def downsample(img):
    return cv2.resize(img, None, fx=0.5,
                      fy=0.5, interpolation=cv2.INTER_NEAREST)


def show_image(img, label='Depth Image', wait_period=0):
    cv2.imshow(label, cv2.resize(img, (1920, 1080),
                                 interpolation=cv2.INTER_NEAREST))
    cv2.waitKey(wait_period)


def keras_format_data(data):
    # return data.astype(float32)
    return data


def load_data(filename):
    f = h5py.File(filename, 'r')
    X_train = keras_format_data(f['train']['features'])
    X_test = keras_format_data(f['test']['features'])
    Y_train = keras_format_data(f['train']['predictions'])
    Y_test = keras_format_data(f['test']['predictions'])

    return (X_train, Y_train, X_test, Y_test)


def create_convnet_model():
    model = Sequential()

    model.add(Conv2D(128, (3, 3), padding='valid',
                     data_format="channels_last",
                     activation='relu', input_shape=(7, 7, 2)))
    model.add(Conv2D(128, (3, 3), padding='valid',
                     data_format="channels_last",
                     activation='relu'))
    model.add(Conv2D(128, (2, 2), padding='valid',
                     data_format="channels_last",
                     activation='relu'))
    model.add(Conv2D(128, (1, 1), padding='same',
                     data_format="channels_last",
                     activation='relu'))
    model.add(Flatten())
    model.add(Dense(100, activation='relu'))
    model.add(Dense(100, activation='relu'))
    model.add(Dropout(0.5))
    model.add(Dense(2 * 2 * 1, activation='linear'))
    model.add(Reshape((2, 2, 1)))

    return model


def create_simple_model():
    model = Sequential()

    model.add(Flatten(input_shape=(7, 7, 4)))
    model.add(Dense(1000, activation='relu'))
    model.add(Dropout(0.1))
    model.add(Dense(1000, activation='relu'))
    model.add(Dropout(0.1))
    model.add(Dense(1000, activation='relu'))
    model.add(Dropout(0.1))
    model.add(Dense(2 * 2 * 3, activation='relu'))
    model.add(Reshape((2, 2, 3)))

    return model


def process_img(X_train_img, Y_train_img):
    new_x = np.zeros(
        (X_train_img.shape[0], X_train_img.shape[1], 2), dtype=float32)
    new_y = np.zeros(
        (Y_train_img.shape[0], Y_train_img.shape[1], 1), dtype=float32)
    new_x[:, :, 0] = cv2.cvtColor(
        X_train_img[:, :, :3], cv2.COLOR_BGR2YUV)[:, :, 0]
    new_x[:, :, 1] = X_train_img[:, :, 3]
    new_y[:, :, 0] = cv2.cvtColor(
        Y_train_img[:, :, :3], cv2.COLOR_BGR2YUV)[:, :, 0]
    #show_image(new_x[:, :, 0])
    # show_image(new_y)
    return (new_x, new_y)


def process_images(X_train_images, Y_train_images, verbose=False):
    new_x = np.zeros(
        (X_train_images.shape[0], X_train_images.shape[1],
         X_train_images.shape[2], 2),
        dtype=float32)
    new_y = np.zeros(
        (Y_train_images.shape[0], Y_train_images.shape[1],
         Y_train_images.shape[2], 1),
        dtype=float32)
    assert(X_train_images.shape[0] == Y_train_images.shape[0])
    for i in range(X_train_images.shape[0]):
        if verbose:
            print("Processed image {}/{}".format(i + 1,
                                                 X_train_images.shape[0]))
        new_x[i], new_y[i] = process_img(X_train_images[i], Y_train_images[i])
    return (new_x, new_y)


def train_model(model, data, save_file='model.h5', weights_file='weights.h5', decimate_factor=5):
    X_train, Y_train, X_test, Y_test = data

    batch_size = 128
    train_data_length = X_train.shape[0] // decimate_factor
    train_data_it = train_data_length // batch_size
    val_start = int(train_data_it * 0.9)

    adam = Adam(lr=0.0001, decay=0.0)
    model.compile(loss='mean_absolute_error',
                  optimizer=adam, metrics=['cosine'])

    def train_batch_generator():
        while True:
            iter_range = [i for i in range(0, val_start)]
            shuffle(iter_range)
            for i in iter_range:
                i_curr = i * batch_size
                X_train_batch = X_train[i_curr:i_curr + batch_size]
                Y_train_batch = Y_train[i_curr:i_curr + batch_size]

                yield process_images(X_train_batch, Y_train_batch)

    def val_batch_generator():
        while True:
            iter_range = [i for i in range(val_start, train_data_it)]
            shuffle(iter_range)
            for i in iter_range:
                i_curr = i * batch_size
                X_train_batch = X_train[i_curr:i_curr + batch_size]
                Y_train_batch = Y_train[i_curr:i_curr + batch_size]

                yield process_images(X_train_batch, Y_train_batch)

    '''model.fit(X_train[:train_data_it * 256], Y_train[:train_data_it * 256], batch_size=256,
              epochs=3, verbose=1, validation_split=0.1)'''
    model.fit_generator(train_batch_generator(),
                        steps_per_epoch=val_start,
                        epochs=3, verbose=1,
                        validation_data=val_batch_generator(),
                        validation_steps=train_data_it - val_start,
                        shuffle=True)
    num_test = X_test.shape[0] // decimate_factor
    processed_test_img = process_images(
        X_test[:num_test], Y_test[:num_test], True)
    score = model.evaluate(
        processed_test_img[0], processed_test_img[1], verbose=1)
    print(score)
    model.save(save_file)
    model.save_weights(weights_file)


def exec_on_image(img, model):
    final_img = np.zeros((img.shape[0] * 2, img.shape[1] * 2, 1), np.float32)
    input_img = np.zeros((img.shape[0], img.shape[1], 2), np.float32)
    input_img[:, :, 0] = cv2.cvtColor(
        img[:, :, :3], cv2.COLOR_BGR2YUV)[:, :, 0]
    input_img[:, :, 1] = img[:, :, 3]
    padded_img = cv2.copyMakeBorder(input_img, 3, 3, 3, 3,
                                    cv2.BORDER_CONSTANT,
                                    value=[0, 0, 0, 0])
    for i in range(3, padded_img.shape[0] - 3):
        for j in range(3, padded_img.shape[1] - 3):
            box = np.reshape(padded_img[i - 3:i + 4, j - 3:j + 4, :],
                             (1, 7, 7, 2))
            print(box[0].shape)
            result = model.predict(box, batch_size=256, verbose=1)
            a_i = i - 3
            a_j = j - 3
            final_img[2 * a_i: 2 * a_i + 2, 2 * a_j: 2 * a_j + 2, :] = result
    return final_img


def test_on_image_pair(color_image_filename,
                       depth_image_filename,
                       model):
    i = 200
    j = 750
    #i = 0
    #j = 0
    r_x = 100
    r_y = 100
    color_img = read_color_img(color_image_filename)

    if isfile(depth_image_filename):
        depth_img = read_depth_img(depth_image_filename)
    else:
        depth_img = np.full((color_img.shape[0], color_img.shape[1]),
                            0.950011444,
                            dtype=np.float32)

    downsampled_depth = downsample(depth_img)
    downsampled_color = downsample(color_img).astype(float32)
    downsampled_depth = \
        np.reshape(downsampled_depth,
                   (downsampled_depth.shape[0], downsampled_depth.shape[1],
                    1)) ** 32
    downsampled_combined = np.append(
        downsampled_color, downsampled_depth, axis=2). \
        astype(float32)[i:i + r_x, j:j + r_y, :]

    high_res_img = color_img[i * 2:i * 2 + r_x * 2,
                             j * 2:j * 2 + r_y * 2, :].astype(np.float32)
    low_res_img = downsampled_combined[:, :, 0:3]
    show_image(cv2.cvtColor(
        high_res_img, cv2.COLOR_BGR2YUV)[:, :, :1])
    show_image(cv2.cvtColor(
        low_res_img, cv2.COLOR_BGR2YUV)[:, :, :1])
    cv2.imwrite("reference_high_res.png", high_res_img * 255)
    cv2.imwrite("original_low_res.png", low_res_img * 255)

    upscaled = exec_on_image(downsampled_combined, model)
    show_image(upscaled)
    cv2.imwrite("upscaled_res.png", upscaled * 255)


def visualize_dataset(data):
    X_train, Y_train, X_test, Y_test = data
    print(X_test)
    for i in range(X_test.shape[0] - 5000, X_test.shape[0]):
        patch = X_test[i]
        high_res = Y_test[i]

        show_image(patch)
        show_image(high_res)


if __name__ == '__main__':
    assert(len(argv) == 2 or len(argv) == 3)
    if argv[1] == 'show':
        assert(len(argv) == 3)
        model = load_model(argv[2])
        test_on_image_pair('color_test.png', 'depth_test.pgm', model)
    elif argv[1] == 'visualize':
        assert(len(argv) == 3)
        data = load_data(argv[2])
        visualize_dataset(data)
    else:
        assert(len(argv) == 2)
        data = load_data(argv[1])
        train_model(create_convnet_model(), data)
