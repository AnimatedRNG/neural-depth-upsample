#!/usr/bin/env python3

import cv2
import numpy as np
from numpy import uint32, float32, sqrt
from sys import argv, exit

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
    cv2.imshow(label, cv2.resize(img, (1600, 900),
                                 interpolation=cv2.INTER_NEAREST))
    cv2.waitKey(wait_period)


def keras_format_data(data):
    return data.astype(float32)


def load_data(filename):
    with h5py.File(filename, 'r') as f:
        X_train = keras_format_data(f['train']['features'][:])
        X_test = keras_format_data(f['test']['features'][:])
        Y_train = keras_format_data(f['train']['predictions'][:])
        Y_test = keras_format_data(f['test']['predictions'][:])

        return (X_train, Y_train, X_test, Y_test)


def create_convnet_model():
    model = Sequential()

    model.add(Conv2D(128, (3, 3), padding='valid',
                     data_format="channels_last",
                     activation='relu', input_shape=(7, 7, 4)))
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
    model.add(Dense(400, activation='relu'))
    model.add(Dense(30, activation='relu'))
    model.add(Dense(2 * 2 * 3, activation='linear'))
    model.add(Reshape((2, 2, 3)))

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


def train_model(model, data, save_file='model.h5', weights_file='weights.h5'):
    X_train, Y_train, X_test, Y_test = data
    adam = Adam(lr=0.001, decay=0.0)
    model.compile(loss='mean_absolute_error',
                  optimizer=adam, metrics=['accuracy'])
    model.fit(X_train, Y_train, batch_size=256,
              epochs=3, verbose=1, validation_split=0.1)
    score = model.evaluate(X_test, Y_test, verbose=1)
    print(score)
    model.save(save_file)
    model.save_weights(weights_file)


def exec_on_image(img, model):
    final_img = np.zeros((img.shape[0] * 2, img.shape[1] * 2, 3), np.float32)
    padded_img = cv2.copyMakeBorder(img, 3, 3, 3, 3,
                                    cv2.BORDER_CONSTANT,
                                    value=[0, 0, 0, 0])
    for i in range(3, padded_img.shape[0] - 3):
        for j in range(3, padded_img.shape[1] - 3):
            box = np.reshape(padded_img[i - 3:i + 4, j - 3:j + 4, :],
                             (1, 7, 7, 4))
            print(box[0].shape)
            result = model.predict(box, 32, 1)
            a_i = i - 3
            a_j = j - 3
            final_img[2 * a_i: 2 * a_i + 2, 2 * a_j: 2 * a_j + 2, :] = result
    return final_img


def test_on_image_pair(color_image_filename,
                       depth_image_filename,
                       model):
    i = 800
    j = 1000
    r_x = 100
    r_y = 100
    depth_img = read_depth_img(depth_image_filename)
    color_img = read_color_img(color_image_filename)

    downsampled_depth = downsample(depth_img)
    downsampled_color = downsample(color_img).astype(float32)
    downsampled_depth = \
        np.reshape(downsampled_depth,
                   (downsampled_depth.shape[0], downsampled_depth.shape[1],
                    1)) ** 32
    downsampled_combined = np.append(
        downsampled_color, downsampled_depth, axis=2). \
        astype(float32)[i:i + r_x, j:j + r_y, :]

    high_res_img = color_img[i * 2:i * 2 + r_x * 2, j * 2:j * 2 + r_y * 2, :]
    low_res_img = downsampled_combined[:, :, 0:3]
    show_image(high_res_img)
    show_image(low_res_img)
    cv2.imwrite("reference_high_res.png", high_res_img * 255)
    cv2.imwrite("original_low_res.png", low_res_img * 255)

    upscaled = exec_on_image(downsampled_combined, model)
    show_image(upscaled)
    cv2.imwrite("upscaled_res.png", upscaled * 255)

def visualize_dataset(data):
    X_train, Y_train, X_test, Y_test = data
    for i in range(5000, 5300):
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
