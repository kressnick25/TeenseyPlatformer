
# AVR Teensey Platformer
This is a platformer game compiled with [avr-gcc](https://ccrma.stanford.edu/~juanig/articles/wiriavrlib/AVR_GCC.html) and run on the [Teensey Microcontroller](https://www.pjrc.com/teensy/).

## AVR Installation

avr-gcc -> Compiler


#### Linux
run
```shell
sudo apt-get update
sudo apt-get install gcc-avr binutils-avr avr-libc
```

### MacOS
install brew
```shell
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

Then run
```shell
brew tap osx-cross/avr
brew install avr-libc
```

## Compiling
Run the make file in each subdirectory of lib/ by running `make rebuild` in each subdir.

Then in the root folder run `make rebuild` to compile the program.

## Flashing Teensey
Use the [Teensey Loader](https://www.pjrc.com/teensy/loader.html) application to flash .hex file to Teensey.
