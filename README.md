# Windows Hello Auto OK for Windhawk

A language-independent Windhawk tool mod that confirms the ready **OK** button in standard Windows Hello biometric dialogs across applications.

## How it works

The mod looks only at a foreground window owned by the Windows copy of `CredentialUIBroker.exe`. It requires a visible `BioFeedbackText` text control, a `CredentialsListV2` list, and exactly one visible, enabled `OkButton` button. Any visible, enabled edit or password field prevents confirmation.

The ready state must persist for the configured delay and is checked again immediately before invoking the button through UI Automation. No keyboard or mouse input is broadcast.

**There are no language lists or caption comparisons.** The code never queries UI names, translated messages, user names, requesting application names or website addresses. Automation IDs are language-neutral. See Microsoft's [UI Automation testing documentation](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-usefortesting).

This covers applications using the standard system dialog structure. It does not depend on any particular browser. An application with its own biometric interface, or a Windows version with different automation IDs, requires separate support.

## Install in Windhawk

1. Open Windhawk and choose **Create a New Mod**.
2. Replace the template with the complete contents of [windows-hello-auto-ok.wh.cpp](windows-hello-auto-ok.wh.cpp).
3. Save, compile and enable the mod.
4. Accept the Windows administrator consent prompt for the dedicated tool host when requested.

The mod follows the [Windhawk tool-mod process model](https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process). It targets Windhawk's own host process. It requests elevation because Windows protects credential dialogs from ordinary processes. If consent is declined, automatic confirmation remains inactive; toggle the mod off and on to retry. Elevation may be requested again after restarting the tool host or signing in to Windows.

The mod creates no scheduled tasks, installs no service, and has no dependency on a separate helper executable. Disable or remove it through Windhawk.

## Settings

| Setting | Default | Meaning |
| --- | --- | --- |
| Enabled | true | Pause or resume automatic confirmation while the mod is loaded. |
| DelayMs | 350 | Required stable readiness period; clamped to 150–3000 ms. |

## Privacy

The source contains no account-specific defaults, personal paths, hostnames, saved credentials, or machine configuration. No account names, credential values, biometric data or interface text are read or logged. Windhawk local storage contains only a generic runtime state and a confirmation counter. The mod makes no network requests.

## Validation and compatibility

This is an **experimental** release targeting the standard Windows Hello dialog. The structural guard is not a separate biometric-success API: Windows remains responsible for authentication, and the mod invokes a confirmation control exposed by that dialog.

Automated tests cover ready, missing, hidden, disabled, ambiguous and credential-input states. Both x86 and x86-64 mod builds are supported for Windhawk 1.7.3. Automated guard tests and compilation do not replace interactive verification across Windows versions, applications and authentication providers. Such end-to-end compatibility is not claimed for this initial release.

Run the structural tests on Windows with CMake and a C++ compiler:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The CI workflow runs these guard tests. Install the single `.wh.cpp` file through Windhawk to compile the actual mod.

## License

[MIT](LICENSE).
