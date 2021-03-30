# dwm - dynamic window manager

![dwm logo](dwm.png)

dwm is an extremely fast, small, and dynamic window manager for X.

## The fork of mine

did some patches and several custom tweeks can't remember

now usin' as a true fork not just patched orig

### Changes in my fork

still tryin' to comment all the things I understood
(fuck you suckless devs and ily ❣️)

made some changes, some refactoring...

idk you'll figure out if you're smart enough.

### Your code style sucks!!

no u

#### Use suckless-like code style

no cuz it sucks

#### Use tabs or at least 4/8/16/32/64/128/thefuck spaces

no.

I like 2 spaces indentation. I think anybody should prefer 2 spaces indentation.
Tabs are bad. No argue.

Readability is not about indentation - it's about good code with empty lines,
comments and consistent style.

### I think your fork is bad because...

u can just fork it and make it "better"

Also you can try and PR your changes, but I don't usually agree with other
people.

If forking it doesn't suit you - gtfo. That's how it works. Simple enough.

## Requirements

`Xlib` header files. That's it.

## Installation

Edit `config.mk` to match your local setup.

Edit `config.h` to configure some of the aspects of your build.

Enter something like the following command to build and install dwm:

    sudo make install && make clean

## Running dwm

Add the following line to your .xinitrc to start dwm using startx:

```sh
exec dwm
```

In order to connect dwm to a specific display, make sure that
the DISPLAY environment variable is set correctly, e.g.:

```sh
DISPLAY=foo.bar:1 exec dwm
```

(This will start dwm on display :1 of the host foo.bar.)

In order to display something in the bar, you should set your root window
name, e.g.:

```sh
xsetroot -name "something"
```

You should use some script to make it dynamic. Also you can use something like
[dwmblocks](https://github.com/torrinfail/dwmblocks).

### My "startdwm script"

`fehbg` is a script for wallpaper render, `autostart` is for starting programs
need to start excluding those that are already running.

```sh
#!/bin/sh

while true; do
  fehbg &
  autostart &
  dwm 2> ~/.dwm.log
done
```
