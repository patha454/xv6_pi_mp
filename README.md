# xv6 for the Raspberry Pi

## Introducing xv6 for the Raspberry Pi

xv6 is a lightweight operating system designed for teaching and researching operating systems.

This Raspberry Pi version of *xv6* is currently being developed by The University of Otago's Systems Research Group (SRG) as an open source project.

xv6 benefits from a short code length: The kernel is approximately 10,000 lines of C which makes it
possible to study the entire kernel, or large sections of the kernel during in one or two semesters. 

xv6 is ideal for introducing operating systems concepts in a practical environment before students are exposed to larger, more sophisticated kernels such as Linux or FreeBSD.

## History

xv6 is based on Sixth Edition Unix - which directly and indirectly influenced the development of many modern operating systems such as Linux and FreeBSD.

MIT's CSAIL laboratory rewrote Sixth Edition Unix in ANSI C for more modern Intel x86 Processor, intending to use the short kernel code for teaching undergraduate students.

The University of Otago SRG further ported xv6 to the cheap, widely available Raspberry Pi single board computer which is based on ARMv7 and ARMv8 processors.

The University of Otago currently develops the Pi port of the xv6 and uses it for teaching undergraduate and postgraduate classes on operating systems.

## Capabilities

xv6 is a relatively limited operating system, which reflects its intended use in education rather than industrial deployment.

xv6 can outperform Linux on some parallel workloads, although we do not envision xv6 being deployed industry.

xv6 has a small subset of classical Unix utilities such as `cat`, `wc`, and `mknod`. xv6 supports pipes and file redirections.

xv6 currently lacks a C standard library, native compiler, or text editor. We welcome contributions of user space utilities via pull requests, of course.

We currently have a C standard library like functions for IO (`printf`, `putc`) in `uprogs/printf.c`, memory management (`malloc`, `free`) in `uprogs/umalloc.c`, and some string operations (`strcmp`, `memset`, `atoi`) in `uprog/ulib.c`.

Unix style system calls (`fork`, `open`, `exec`...) are available in `uprogs/user.h`.

## Acknowledgements

We would like to thank the MIT CSAIL for their work on the x86 version of xv6, which inspired our use of xv6 at The University of Otago.

The original ARM port of xv6 was developed my Associate Professor Zhiyi Huang (zhy@cs.otago.ac.nz).

Mahdi Amir Kordestany (University of Otago) ported xv6 from the Raspberry Pi to the Raspberry Pi 2/3.

Harley Paterson (University of Otago) implemented multiprocessing, and compatibility for the ARMv8 CPUs used on later Pi 3 models.

## Contributing.

If you spot errors or want to contribute improvements, please send email to Zhiyi Huang (hzy@cs.otago.ac.nz), or make a pull request.

## Building and Running xv6

xv6 is easy to get started.

1. Clone to source code to your Pi:

```
$ git clone https://github.com/patha454/xv6_pi_mp.git`
$ cd xv6_pi_mp/
```

2. Build the user space programs into a ramdisk:

```
$ cd uprogs/
$ make
```

3. Copy the user space programs to the main directory:

```
$ cp initcode fs.img .. 
```

4. Compile the kernel and build the disk image from the main directory:

```
$ cd ..
$ make
```

5. Add the kernel image to your /boot/ directory:

```
$ cp kernel7.img /boot/xv6_kernel.img
```

6. Configure the Raspberry Pi bootloader to load xv6 by opening `/boot/config.txt` and comment out the line `kernel=kernel-xv6.img` by adding a `#` before it, which loads the Linux kernel.

Add the following entries to `/boot/config.txt`:

```
kernel=xv6_kernel.img
kernel_old=1
disable_commandline_tags=1
enable_uart=1
```

7. Reboot your Pi. The Pi should now boot into xv6. 

You can get back to Linux by power cycling the Pi (xv6 does not have a shutdown option yet). 

Press `ESC` when presented with the bootloader splash screen to bring up the bootloader options.
Click the "Edit config" button and comment out the lines you added in part 6. Uncomment `kernel=kernel-xv6.img` to restore Linux.
