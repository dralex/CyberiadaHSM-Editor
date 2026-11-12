# a counter of the events
count = 0
for event in ("TICK", "TICK", "RESET"):
    count = 0 if event == "RESET" else count + 1
print(count)
