# convert -resize 96x192! large.png png24:tftfont.png
convert -resize 96x208! large.png png24:tftfont.png
./convert-font tft tftfont.png tftfont.h
