ML-driver
=========

The **ML-driver** is a proof-of-concept Linux kernel driver for the DreamCheeky
USB missile launcher. I developed it mainly to gain experience in kernel driver
hacking. Hence it is mereley a skeleton and not very sophisticated.

For more information about how the driver has been created, have a look at
the [article][blog-post] which describes the development process in detail.

[blog-post]: http://matthias.vallentin.net/blog/2007/04/writing-a-linux-kernel-driver-for-an-unknown-usb-device/

Compatibility
=============

Linux:
  - **3.16**

Tested on:
  - *Ubuntu 3.16.0 x86_64* (by [mrquincle](https://github.com/mrquincle))

Usage
=====

First compile the module:

    make

To load the module, I provide a little script:

    ./ml_manage.sh load

Check your messages afterwards. If everything went fine, you should see these
lines:

    [info]  ml_probe(593): USB missile launcher now attached to /dev/ml0
    usbcore: registered new driver missile_launcher
    [info]  usb_ml_init(651): driver registered successfully


Now you are ready to play with the tool in user-space. Simply compile the
example code in this directory:

    gcc -o ml_control user-space.c
    ./ml_control -f

Double-check the permissions of your device (`/dev/ml0`) when encountering any
problems.


License
=======

This kernel module comes with a GPL license.
