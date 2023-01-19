
UINT16_MAX = 65535
INT16_MAX = 32767

scale = 0

val1 = 0
val2 = 100

I = (val1*scale + val2*(UINT16_MAX+1-scale)) >> 16

print(I)