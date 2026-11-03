# Code signing policy

## Status

The published Windows releases are currently **unsigned**. Code signing through
SignPath Foundation is planned, subject to application approval and eligibility
review. This policy does not mean that a certificate or signed release is already
available.

If approved: Free code signing provided by [SignPath.io](https://signpath.io/),
certificate by [SignPath Foundation](https://signpath.org/).

This Windows port is maintained in
[DongwonSohn/linthesia-windows](https://github.com/DongwonSohn/linthesia-windows).
It derives from [Linthesia](https://github.com/linthesia/linthesia); this repository
was created as a standalone copy rather than through GitHub's Fork feature.
Signed upstream releases have not been verified. Downstream eligibility and
bundled resource licensing must be resolved before Foundation signing is enabled.

## Team roles

- Author and committer: [DongwonSohn](https://github.com/DongwonSohn).
- Reviewer of outside contributions: [DongwonSohn](https://github.com/DongwonSohn).
- Release signing approver: [DongwonSohn](https://github.com/DongwonSohn).

Maintainers must use multi-factor authentication for GitHub and SignPath access
before signing is enabled. Outside contributions, including build and workflow
changes, require maintainer review. Each release signing request requires manual
approval by the designated approver.

## Signing scope and build provenance

Only these first-party Windows binaries are intended for signing:

- `Linthesia.exe`, the portable launcher.
- `dll/LinthesiaApp.dll`, the application module.

Bundled third-party runtime DLLs and test binaries are outside this project's
signing scope. Source and dependency licenses remain applicable; a signature does
not grant rights to bundled music or other resources.

Release signing must use artifacts built from public source by the
[Windows executable workflow](https://github.com/DongwonSohn/linthesia-windows/actions/workflows/windows.yml)
on GitHub-hosted runners, with SignPath's build-origin verification. Local and
cross builds are not submitted as Foundation release artifacts. Both first-party
binaries must identify the product as `Linthesia`, carry the same product version,
and pass the Windows package checks. After signing, signatures must be validated
and the final ZIP checksum regenerated before publication. Signed files must not
be changed after signing.

## Privacy

The application does not send telemetry or upload MIDI files, settings, or song
statistics. It reads local files and communicates with MIDI devices selected by
the user. See the [Privacy policy](PRIVACY.md) for details, including local data
removal and external services.
