.section .rodata
.balign 4


.global picData
.global picEnd

.type picData, %object
.type picEnd, %object

picData:
.incbin "src/picture.bin"
picEnd: