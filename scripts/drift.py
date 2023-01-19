
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

N = 32
M = 1024

R = np.random.randint(-32, 32, N)

F = R**3
F_scale = np.empty(M)

prev = 0
for i, r in enumerate(F):
    m = M//N
    for j in range(m):
        F_scale[i*m+j] = prev + (r-prev)*(j/m)
    prev = r


b, a = signal.iirfilter(2, Wn=0.1, btype="low", ftype="butter")
Y = signal.lfilter(b, a, F_scale)

plt.plot(Y)
plt.show()
