#! /bin/bash
#
ftp -n -v webcam.wunderground.com << EOT
ascii
user <camera-ID> <WU-password>
type binary
lcd /home/pi/Images
put latest-image.jpg
bye
EOT
