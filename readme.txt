Welcome to the source code of the CSurf_Klinke_MCU Extension for
Reaper (v3.6 or higher). This extension enhances support for the
Mackie Control Universal (MCU) controller, but can also be used for
other controllers that use the MCU protocol (please read the manual of
your controller to check if and how your controller supports this
protocol).

Beside the Reaper SDK the code also depends on the JUCE and BOOST
libraries. So you will need to indicate three SDK locations via
environment variables. Sometimes the JUCE library is not backward
compatible, therefore you need v1.50 or v1.52!

Example: The juce SDK (v1.52) is installed in C:\juce and the
header file is found in C:\juce\juce.h

Set the environment variable in 

Start -> MyComputer -> RightClick -> Properties -> Advanced ->
Environment Variables -> System Variables section -> New Button

Variable Name = JUCE
Variable Value = C:\juce

Of course your actual paths (Variable Values) may be different, but
the Variable Names must exactly agree with the ones above, those
Variable Names are the ones referenced in the Visual Studio project.

For the reaper extensions SDK repeat as above:

Example: C:\reaper_extension_sdk

Variable Name = REAPER_EXTENSION_SDK
Variable Value = C:\reaper_extension_sdk

And for the Boost Library (v 1.39.0 or above) repeat again as above:

Example: C:\boost

Variable Name = BOOST
Variable Value = C:\boost

All parts of the Boost Library are header-only based, so it's not
necessary to compile the libs.

> **This section is superseded.** The project now uses JUCE 8, Boost 1.91,
> and a cross-platform CMake build. See AGENTS.md for current build
> instructions. The old JUCE 1.52 / VS2010 build files have been archived
> to archive/vs-legacy/ and archive/juce-1.52-patches/.


