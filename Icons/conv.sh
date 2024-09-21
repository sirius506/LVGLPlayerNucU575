for i in *.png
do
  echo $i
  ../lvgl/scripts/LVGLImage.py --ofmt BIN --cf RGB565A8 $i
done
