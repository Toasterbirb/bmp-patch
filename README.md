# bmp-patch

Fix partially corrupted bmp files. Usage: `bmp-patch ./path_to_a_bmp_file`

## Building
Build the project with g++ by running `make`. To speed up the build, you can try using the -j flag.
```sh
make -j$(nproc)
```

## Installation
To install bmp-patch to /usr/local/bin, run the following
```sh
make install
```
You can customize the installation prefix with the PREFIX variable like so
```sh
make PREFIX=/usr install
```

## Uninstall
```sh
make uninstall
```
