Hello, welcome to the scripts directory!

'rebuild' is used to signify deleting build folder and starting fresh.
'build' is used to represent a recompile of the modified build target.

I'm not sure what exactly is done under the hood on this, I believe it is handled by CMake.
Have a look at the scripts and you will see the difference.  I simply don't do the 'rm -rf build' 'mkdir build' steps in the scripts

I made these scripts to easily call these actions from .vscode tasks, for which I can create hotkeys.  
I didn't have the scripts before, but I thought hey, what if someone has xcode or Vim?

Also, it's annoying to change directories all the time for calling cmake commands.