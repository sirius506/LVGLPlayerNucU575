#!/bin/bash
for i in *.png; do
 LVGLImage.py --ofmt C --cf RGB565A8 $i
done
