# vt-firmware-pack

An program that contains a set of Vertigris stable firmware. After installation, the program will push the package firmware list to AWS shadow's desired object. That will further trigger the package management behavior of vt-bpm. To update the vt-firmware to desired versions.



The program is built on vt-fitness, which itself is based on [the AWS IoT SDK for C++ v2](https://github.com/aws/aws-iot-device-sdk-cpp-v2).

[//]: # (Image References)

[image1]: ./images/vt-firmware-pack.png "Design Overview"


## Development Requirements

The only requirement is [docker](https://www.docker.com/) because the whole development environment is created in a docker image. Just use the following commands to start coding:

```
# Create a docker image and run it.
make run_container

# Enter the created docker container.
make debug
```

As long as you are inside the container, you can use the commands below to build the code and run the test cases:
```
mkdir build
cd build
cmake .. && make    # builds the code and runs the linter
ctest --verbose     # runs the test cases
```

## How to build the debian package

The code is built inside a docker container, which means you must have `docker` installed on your system. Run commands below will create the debian package that can be installed on a device:

```
make run_container
docker exec vt-fitness bash -c 'make'
```

Then, you can find the package in the directory `out/`.

## Design

The design overview is depicted as follows:

![alt text][image1]
