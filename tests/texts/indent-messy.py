import os


def blink(pin, times,
        delay=0.5):
    """Blink the pin.
   The docstring lines are kept."""
    for i in range(times):
            set_pin(pin, True)
            sleep(delay)
              set_pin(pin, False)
       if i == times - 1:
            return i
          else:
            sleep(delay)
    return times


class Lamp:
  def __init__(self, pin):
      self.pin = pin
      self.state = [0,
         1]
      self.name = ('lamp'
     'of the pin')
