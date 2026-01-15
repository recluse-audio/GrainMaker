# GrainMaker

This is a general granular synthesis platform for all sorts of audio transformations.
It includes JUCE and my own RD (Ryan's DSP) libraries as submodules.

This project is heavily test-driven and will remain largely written by human hands.
Although I use my fair share of claude code and chatgpt, but all is written/read by me.

After cloning, be sure to run ```git submodule update --init --recursive``` to get the submodules.

To build you must first run:
`python /SCRIPTS/rebuild_all.py`

Then you can build your desired target:
`python /SCRIPTS/build_tests.py` or `python /SCRIPTS/build_vst3.py`

Built plug-in targets can be found in `BUILD/GrainMaker_Artefacts/Debug/VST3` (changes based on build config, but general idea)
Built Catch2 Tests target is found at `BUILD/Debug/Tests.exe`

Until this README is updated, this project is in-progress. Open to suggestions!


