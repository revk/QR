# QR
QR code generation C library and command line.

Efficient output, and wide variety of output formats including png, svg, eps, ps, text, binary, hex, KiCad module, and even a data URL format for png. See --help for more info.

Command line is aimed at linux, but the library has been used sucessfully under ESP IDF and other platforms.

## Building on Linux

```
$ git clone https://github.com/revk/QR.git 
$ git submodule init
$ git submodule update
$ make
```

## Example usage

```
$ ./qr --svg "Hello there"  > out.svg
```

You can view the output in, eg, firefox with `firefox out.svg`.

## Options

The library allows custom padding in the QR code, for "hidden" data, but also provides mapping to the actual pixels so you can adjust padding to make patterns in the final bar code which are still correct ECC. E.g.

![space-invaders](https://WWW.ME.UK/spaceinvader.png)

It includes a couple of non standard formats, cicles instead of squares, and even a truchet version. Mostly these read.

<img width="1230" height="1230" alt="qr90" src="https://github.com/user-attachments/assets/de18d490-f3de-4031-8a71-d8bb4daa326e" />

There is even a coloured PNG format to allow you to see what parts are data, ecc, padding, control, etc.

![colour-qr-code](https://WWW.ME.UK/exampleqr.png)
