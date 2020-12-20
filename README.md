image_to_c
----------

A command line tool for turning binary image files into C source code. The output is an array of unsigned chars and is sent to stdout. Included are comments detailing the image type, size and other details.<br>
<br>
<b>Why did you write it?</b><br>
My existing tool (bin_to_c) is similar in that it generates C arrays to compile file data directly into a project. I have used this tool to create many .H files to include with my projects, but the filename alone isn't enough to know the details of the image file contained in the data. Instead of manually adding this information to each file, I came up with the idea of combining my imageinfo tool with the bin_to_c tool to make something even more useful.<br>
<b>What does the output look like?</b><br>
Here's an example of a before and after of what this new tool does:<br>

It turns this type of file:<br>
![Animated GIF](/badger.gif?raw=true "Animated GIF")

Into this type of file:<br>
![screenshot](/screenshot.png?raw=true "screenshot")

<b>What image file types does it support?</b><br>

PNG, JPEG, BMP, TIFF, GIF, PPM, TARGA, JEDMICS, CALS and PCX<br>

<b>What happens for unrecognized files?</b><br>
If the file type is not known, it will generate the same C output, but without additional info.<br>

If you find this code useful, please consider sending a donation or becoming a Github sponsor.

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SR4F44J2UR8S4)

