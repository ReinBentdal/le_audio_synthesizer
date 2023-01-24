
import math

UINT16_MAX = 32767
N = 256

Y = []
for n in range(N+1):
    x = 2*math.pi*n/N
    y = round(UINT16_MAX*math.sin(x))
    Y.append(y)

print('{', end='')
print(','.join(map(str, Y)), end='')
print('};')