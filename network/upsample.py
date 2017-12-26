#!/usr/bin/env python3

import cv2
import numpy as np
from numpy import uint32, float32, sqrt
from sys import argv, exit

import h5py

import pandas
from keras.models import Sequential
from keras.layers import Dense, Dropout, Activation, Flatten, Reshape
from keras.layers import Conv2D, MaxPooling2D
from keras.layers import GaussianNoise, GaussianDropout
from keras.wrappers.scikit_learn import KerasRegressor
from sklearn.model_selection import cross_val_score
from sklearn.model_selection import KFold
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline


def keras_format_data(data):
    # return np.rollaxis(data, 3, 1).astype(float32)
    return data.astype(float32)


def load_data(filename):
    with h5py.File(filename, 'r') as f:
        '''X_train = np.rollaxis(f['train']['features'][:], 3, 1).astype(float32)
        Y_train = np.rollaxis(f['train']['predictions']
                              [:], 3, 1).astype(float32)
        X_test = np.rollaxis(f['train']['features'][:], 3, 1).astype(float32)
        Y_test = np.rollaxis(f['test']['predictions'][:], 3, 1).astype(float32)'''

        X_train = keras_format_data(f['train']['features'][:])
        X_test = keras_format_data(f['test']['features'][:])
        Y_train = keras_format_data(f['train']['predictions'][:])
        Y_test = keras_format_data(f['test']['predictions'][:])

        return (X_train, Y_train, X_test, Y_test)


def create_convnet_model():
    model = Sequential()

    model.add(Conv2D(96, (3, 3), padding='valid',
                     data_format="channels_last",
                     activation='relu', input_shape=(7, 7, 4)))
    model.add(Conv2D(128, (3, 3), padding='valid',
                     data_format="channels_last",
                     activation='relu', input_shape=(5, 5, 96)))
    model.add(Conv2D(256, (2, 2), padding='valid',
                     data_format="channels_last",
                     activation='relu', input_shape=(3, 3, 128)))
    model.add(Conv2D(256, (1, 1), padding='same',
                     data_format="channels_last",
                     activation='relu', input_shape=(2, 2, 256)))
    model.add(Flatten(input_shape=(2, 2, 256)))
    model.add(Dense(128, activation='relu', input_shape=(2 * 2 * 256,)))
    model.add(Dense(2 * 2 * 3, activation='relu', input_shape=(128,)))
    model.add(Reshape((2, 2, 3), input_shape=(2 * 2 * 3,)))

    return model


def create_simple_model():
    model = Sequential()

    model.add(Flatten(input_shape=(7, 7, 4)))
    model.add(Dense(1000, activation='relu'))
    model.add(Dropout(0.5))
    model.add(Dense(2 * 2 * 3, activation='relu'))
    model.add(Reshape((2, 2, 3)))

    return model


def train_model(model, data):
    X_train, Y_train, X_test, Y_test = data
    model.compile(loss='mean_squared_error',
                  optimizer='adam', metrics=['accuracy'])
    model.fit(X_train, Y_train, batch_size=256,
              epochs=10, verbose=1, validation_split=0.3)
    score = model.evaluate(X_test, Y_test, verbose=1)
    print(score)


if __name__ == '__main__':
    assert(len(argv) == 2)
    data = load_data(argv[1])
    '''cv2.imshow('Img', cv2.resize(
        data[0][0], (3200, 1800), interpolation=cv2.INTER_NEAREST))
    cv2.waitKey(100)
    cv2.imshow('Img', cv2.resize(
        data[1][0], (3200, 1800), interpolation=cv2.INTER_NEAREST))
    cv2.waitKey(100)'''
    train_model(create_convnet_model(), data)
