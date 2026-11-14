import time

class Light:
    def __init__(self):
        self.n = 0

    def step(self):
        self.n += 2
        self.log()
        return self.n

    def reset(self):
        self.n  =  0

    def name(self):
        return "lamp"
