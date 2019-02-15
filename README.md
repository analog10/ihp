# ihp: "Intel Hex" Parser #
Note: this library is not endorsed or developed by Intel; the name Intel is used only in reference to the format name.

Description
---------

Callback based Intel Hex file parsing, analogous to SAX for XML.
If you don't know why `ihp` would be useful, go use <a href="http://srecord.sourceforge.net/">srecord</a>.
If still need to parse intel hex directly and you don't understand why you would use `ihp`, then go use <a href="https://github.com/martin-helmich/libcintelhex">libcintelhex</a>. Then come back to `ihp`.
The API is composed of 2 levels

 * **ihp**: Callback functions receiving a start address and data buffer
 * **ihpa**: Higher level constructs based on ihp
     * fill: Traditional data load into binary image, with padding (see ihp_fill_test.c)
     * range: Address based dispatching to specific callback functions (see ihp_test.c)

Build
---------

Just run make. You don't need anything more new or complex. You can install the headers and libraries to your system if you are old school, or you can do the modern copypasta technique. I don't care which you do, unless you do something cool like integrating with a package manager. Let me know about that please.
