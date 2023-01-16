
N = 128

print('{', end='')
for n in range(N):
    freq = 440 * 2 ** ((n - 69) / 12)
    print(freq, end='')
    if n != N-1:
        print(', ', end='')
print('};')