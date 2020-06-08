# DCLEDMatrix

This is just a small set of tools to control a "Dream Cheeky USB LED Message Board", somewhat similiar to
[dcled by Jeff Jahr)](https://github.com/kost/dcled) (from which the font was taken). However, the code
for this was written from scratch. In contrast to the existing projects, this implements a simple daemon
which exposes some simple interface via shared memory, so that clients cann control the content of the
LED message baord. 

Currently, I resurrected this old project to use this as some status display for [OBS-Studio](https://github.com/obsproject/obs-studio), and an OBS plugin is included here. 

The rest of the code (especially `dclmclient`) is rather incomplete at this point.

## Requirements

This is written only with Linux in mind. It will use the `libhidapi` Interface to talk to the USB device,
and the OBS Studio development files are required to build the OBS plugin.

## License

Copyright (C) 2011 - 2020 by derhass <derhass@arcor.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

See the file `LICENSE` for details.
