import math
import random

# Gradients for perlin noise
def generate_gradients(n):
    gradients = []
    for i in range(n):
        angle = 2 * math.pi * i / n
        gradients.append((math.cos(angle), math.sin(angle)))
    return gradients

# Randoms for cellular noise
def generate_randoms(n):
    randoms = []
    for i in range(n):
        randoms.append((random.uniform(-1, 1), random.uniform(-1, 1)))
    return randoms

# Generate gradient table with 64 directions
GRADIENT_COUNT = 64
gradients = generate_gradients(GRADIENT_COUNT)
print(f"Perlin noise gradients [{GRADIENT_COUNT * 2}]:")
for i, (gx, gy) in enumerate(gradients):
    print(f" {gx:.17f}, {gy:.17f},", end="")
    if (i + 1) % 4 == 0:  # 2 pairs (4 values) per row
        print()
print()

RANDOM_COUNT = 256
randoms = generate_randoms(RANDOM_COUNT)
print(f"Cellular noise randoms [{RANDOM_COUNT * 2}]:")
for i, (gx, gy) in enumerate(randoms):
    print(f" {gx:.17f}, {gy:.17f},", end="")
    if (i + 1) % 4 == 0:  # 2 pairs (8 values) per row
        print()
print()