# dwm-window-capture
A small window utility that captures the currently focused window using 
Direct3D 11 and DWM redirection surfaces, then save it as a png.

### What it does
 - Captures gpu accelerated windows without BitBlt
 - Uses DWM redirection surfaces
 - Copies shared surface to a staging texture
 - Reads back pixels to CPU
 - Encodes to PNG with WIC

### Requirements
 - Windows
 - Visual studio + windows SDK
 - Direct3D 11
 - C++17 or later
 - Only tested on windows 10 22h2

### Usage
Run the exe, press Alt + Insert to capture
The image will be stored in the working directory in png format

### Notes/warnings
 - Uses undocument DWM exports
 - May break on newer windows versions
 - May capture some protected windows

### Why?
Just wanted to learn more about Clean D3D11 usage and 
windows internals. Also have been putting off finishing
this project for a while, wanted to get it done.
