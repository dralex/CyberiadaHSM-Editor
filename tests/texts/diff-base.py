import time

class Light:
    def __init__(self):
        self.n = 0

    def step(self):
        self.n += 1
        return self.n

    def reset(self):
        self.n = 0
        print("reset")

    def name(self):
        return "light"
