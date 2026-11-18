# Third-Party Notices

This project is distributed under the GNU Affero General Public License v3.0
(AGPLv3). See `LICENSE`.

## JUCE

- Project: https://github.com/juce-framework/JUCE
- Version: 9.0.5
- Licence: AGPLv3 or commercial JUCE licence

JUCE is dual-licensed. Open-source use under AGPLv3 is permitted. Closed-source
distribution requires a commercial JUCE licence from Raw Material Software.

## Catch2

- Project: https://github.com/catchorg/Catch2
- Version: v3.16.1
- Licence: Boost Software License 1.0

## VST3 SDK

- Project: https://github.com/steinbergmedia/vst3sdk
- Licence: dual-licensed under GPLv3 and the Steinberg VST3 licence.

The VST3 SDK is bundled with JUCE for VST3 builds. Review Steinberg's terms
before distributing binaries.

## FFmpeg (planned, export milestone)

- Project: https://ffmpeg.org
- Version: 9.0.2 (planned)
- Licence: LGPLv2.1+ by default; GPLv2+ when built with `--enable-gpl`
  (libx264 for H.264 encoding).

H.264 patent licensing is separate from copyright licensing and may be required
for distribution in some jurisdictions.

## Fonts

The UI uses Nunito exclusively, embedded in the binaries as two static faces:

- Project: https://github.com/googlefonts/nunito
- Files: `Assets/fonts/NunitoBold.ttf` (weight 700), `Assets/fonts/NunitoBlack.ttf` (weight 900)
- Licence: SIL Open Font License 1.1, full text in `Assets/fonts/OFL.txt`

The two faces are instanced from the upstream variable font at weights 700 and
900. Normal copy is set in Nunito Bold and emphasised copy in Nunito Black. The
OFL permits bundling and redistribution with the licence text; the fonts are not
sold on their own and their names are not used for modified derivatives.
