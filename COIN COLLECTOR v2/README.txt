CAR COLLECTOR - PSP PORT
========================

1. ASSETS (put these in an "assets" folder next to main.c)
   car.png  coin.png  jerrycan.png  gem.png   <- gem must be PNG (convert from gem.gif)
   music1.mp3  ding.wav  coin.wav  bonus.wav  gameover.wav
   font.ttf   <- any TrueType font (e.g. DejaVuSans-Bold.ttf); PSP has no "Arial"

   Renamed from your originals:
     "jerry can.png"        -> jerrycan.png
     "bonus coin sound.wav" -> bonus.wav
     "game over.wav"        -> gameover.wav
     gem.gif                -> gem.png

2. BUILD (WSL2 Ubuntu on Windows, with pspdev installed)
   cd carcollector-psp
   make
   Output: EBOOT.PBP

3. INSTALL
   Copy EBOOT.PBP + the assets folder into  PSP/GAME/CarCollector/
   on the Memory Stick. Requires custom firmware.
   You can also open EBOOT.PBP in the PPSSPP emulator on Windows to test first
   (run it from a folder that contains the assets folder).

CONTROLS: D-pad or analog nub = drive, START = pause, X = start/resume/restart
