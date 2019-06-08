# Getting Started

This document briefly describes how to install and use the code.


## Environment

We conducted experiments in the following environment:
 - Linux
 - Python 3
 - FFmpeg

Similar environments (e.g. with OSX, Python 2) might work with small modification, but not tested.


## Description

This is a python wrapper that directly takes a compressed video and returns the HMP representation as a numpy array.


#### Supported video format

Currently we only support MPEG-1/2 raw videos. Other codecs, e.g. MPEG-4 and H.264, coming soon. 
The MPEG-2 raw videos can be obtained using FFmpeg:

`ffmpeg -i input.mp4 -q:v 1 -c:v mpeg2video -f rawvideo output.mpg`


#### Install

 - Download FFmpeg (`git clone https://github.com/FFmpeg/FFmpeg.git`).
 - Go to FFmpeg home,  and `git checkout 864fdfa0627e21ee0b69e957c3413114185623a7`.
 - `make clean`
 - `patch -p1 < ../ffmpeg-864fdfa.patch`
 - `./configure --prefix=${FFMPEG_INSTALL_PATH} --enable-pic --disable-yasm --enable-shared`
 - `make`
 - `make install`
 - If needed, add `${FFMPEG_INSTALL_PATH}/lib/` to `$LD_LIBRARY_PATH`.
 - Go to `hmp` folder.
 - Modify `setup.py` to use your FFmpeg path (`${FFMPEG_INSTALL_PATH}`).
 - `./install.sh`


#### Usage

The python wrapper has one function: `extract` for extracting the HMP representation.

The following call returns the HMP representation of a MPEG-2 raw video.
```python
from hmp import extract
extract([input])
```
 - input: path to video (.mpg).

For example, 
```
extract('input.mpg')
```
