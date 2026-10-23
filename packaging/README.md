# GUI packaging

Configure with `AI_INTERVIEW_BUILD_GUI=ON`, build, test, then run:

```bash
cpack --config build/gui/CPackConfig.cmake -C Release
```

Outputs:

- Windows: ZIP and NSIS installer; Qt runtime is deployed with `windeployqt`/Qt deploy support.
- macOS: `.dmg` containing an `.app`; Qt frameworks are deployed and microphone permission is declared in `Info.plist`.
- Linux: `.tar.gz` and `.deb`; desktop entry is installed under `share/applications`.

Platform packages must be produced on their target operating system. The cross-platform workflow builds, tests, packages, and uploads each artifact independently.
