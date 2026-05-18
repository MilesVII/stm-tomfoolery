.section .rodata
.balign 4


.global rleData
.global rleEnd

.type rleData, %object
.type rleEnd, %object

rleData:
.incbin "src/rle.bin"
rleEnd: