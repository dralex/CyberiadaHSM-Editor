# Blinking a LED

The sketch (a loop of its own) runs on the board:
	a tabbed line of the text {kept}.

```cpp
void setup() {
pinMode(LED, OUTPUT);
}

void loop() {
	if (ready) {
digitalWrite(LED, HIGH);
      delay(1000);
	}
}
```

1. The dispatcher in Python:

   ```python
   def blink(pin, times):
   for i in range(times):
         set_pin(pin, True)
   return times
   ```

2. The settings:

   ~~~json
   {
   "pin": 13,
	"times": [1,
   2]
   }
   ~~~

An untagged block and a shell one stay as they are:

```
  odd {
      lines
```

```bash
if [ -x run ]; then
./run
fi
```
