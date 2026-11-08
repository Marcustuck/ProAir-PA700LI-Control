# Third-party dependencies and notices

No dependency source, prebuilt firmware, or external library binary is bundled here.
Install the tested dependencies separately and retain their upstream notices when
redistributing their code or compiled artifacts. This file does not grant a licence
for the project's original source.

## Arduino ESP32 board package

Tested with the official Arduino ESP32 Boards package 2.0.18-arduino.5.
The installed WiFi and HardwareSerial headers carry GNU LGPL 2.1-or-later notices;
the complete board package also contains components under other upstream terms.
Consult the package's source and notices, rather than assuming one licence covers
all of its dependencies.

WiFi header attribution: Copyright (c) 2011–2014 Arduino; modified by Ivan Grokhotkov.
HardwareSerial header attribution: Copyright (c) 2006 Nicholas Zambetti, with later
upstream modifications. These are upstream dependency attributions, not project authorship.

Upstream sources: https://github.com/arduino/arduino-esp32 and
https://github.com/espressif/arduino-esp32

## PubSubClient

Tested with PubSubClient 2.8. Upstream: https://github.com/knolleary/pubsubclient

The following is its upstream MIT notice, included for attribution only. It applies
to PubSubClient, **not automatically to this project**:

Copyright (c) 2008-2020 Nicholas O'Leary

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

