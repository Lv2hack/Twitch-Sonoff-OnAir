# Twitch-Sonoff-Streaming
Sonoff basic repurposed into "on air" light for specific Twitch user

Uses two files, one is uploaded to the Sonoff basic and the other runs on a Python 3 capable host.  
Apparently, the Twitch API is too hard to implement in Arduino code.  Well, until somebody actuall does it.

The Python script was created by Robotto and is here: https://github.com/Robotto/twitchOnAir 

### Todo:
- Modify Python to check user every 10 sec without ESP connecting.  It would then cache the response and when the ESP checks in just send the 1 or 0.  This way multiple ESPs can connect to the script without exceeding the API rate limit. 
