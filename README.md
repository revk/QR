# QR
QR code generation C library and command line.

Efficient output, and wide variety of output formats including png, svg, eps, ps, text, binary, hex, KiCad module, and even a data URL format for png. See --help for more info.

Command line is aimed at linux, but the library has been used sucessfully under ESP IDF and other platforms.

The library allows custom padding in the QR code, for "hidden" data, but also provides mapping to the actual pixels so you can adjust padding to make patterns in the final bar code which are still correct ECC. E.g.

![space-invaders](https://WWW.ME.UK/spaceinvader.png)

It includes a couple of non standard formats, cicles instead of squares, and even a truchet version. Mostly these read.

There is even a coloured PNG format to allow you to see what parts are data, ecc, padding, control, etc.

![colour-qr-code](https://WWW.ME.UK/exampleqr.png)
