# filter

`filter` will run an IIR filter forward in time across a binary file. The file
may have any number of channels, and is assumed to be arranged in row-major (or
C) order.

To accommodate large files, the dataset is read, filtered, and written in
chunks.

The file is modified in-place.

Filter coefficients must be provided directly. The number of `-a` and `-b`
coefficients must be the same.

Note that an FIR filter can be applied by setting all `-a` (autoregressive)
coefficients to `0.0` except for the first, which is set to `1.0`.

The beginning of the file is zero-padded for the filter's initial state.

## Usage

```
$ filter [file] [channels] -a a0 [a1 a2 ...] -b b0 [b1 b2 ...]
```
 
