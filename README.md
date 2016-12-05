# DGMVCSourceVS
VapourSynth port of DGMVCSource

## Requirements:

Windows 7 or newer

Intel Media SDK

Make sure the included libmfxsw32.dll(or libmfxsw64.dll) is in the same directory as DGMVCSourceVS.dll.

## Usage

Example Code
```
  import vapoursynth as vs

  core = vs.get_core()

  # core.std.LoadPlugin(r"DGMVCSourceVS.dll")

  src = core.DGMVC.DGMVCSource("xxx.264", "xxx.mvc", view = 0, frames = 233333, mode="auto")

  right = core.std.SelectEvery(src, cycle=2, offsets=0)
  left  = core.std.SelectEvery(src, cycle=2, offsets=1)

  sbs = core.std.StackHorizontal([left, right])

  sbs = sbs[:1500]

  sbs.set_output()
```

* single source clip is the combined MVC elementary stream, made with eac3to and mvccombine,
  or dual source clips are the base and dependent elementary streams.
* if a single source clip is specified that is a normal AVC file (not a combined MVC file),
  then it will be correctly treated as a normal AVC file, and the view parameter is ignored.
* view: 0 = interleaved left and right, 1 = left only, 2 = right only.
* frames: number of frames, if too few the stream is truncated, if too many extra black frames are returned
  The specified frames parameter is internally doubled when delivering interleaved, i.e., when view=0.
* mode chooses mode of decoding: "auto" (default) - use HW acceleration if it is available, otherwise use SW,
  "sw" - force SW decoding, "hw" - force HW acceleration, fail if it is not available. If mode is
  not specified, then "auto" is assumed.
* multiple instantiation in a single script is supported to the limit of available resources.

## Limitations 

No seeking, linear access only except for selecteven/selectodd both present as shown above for the Half SBS example
