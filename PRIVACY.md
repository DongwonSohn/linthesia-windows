# Privacy policy

This policy describes the portable Windows application maintained in
[DongwonSohn/linthesia-windows](https://github.com/DongwonSohn/linthesia-windows).

## Local application data

Linthesia reads MIDI files and music folders that you open, along with the bundled
graphics, fonts, and music. It stores preferences, recently used folder paths,
device selections, and a song statistics database locally. The default Windows
data folder is `%APPDATA%\Linthesia`; a previously configured database path may
point elsewhere.

The application has no account registration, advertising, analytics, crash-upload
service, automatic update check, or application-initiated network upload. It does
not send MIDI files, paths, settings, or statistics to the project maintainers or
to SignPath. Bundled libraries are used locally for graphics, fonts, images,
database access, and device access.

## MIDI devices and external services

The application receives and sends MIDI events through the input and output
devices you select. A separately installed virtual or network MIDI driver may
route those events elsewhere according to that driver's settings and policies.

Downloading releases from GitHub and visiting the linked SignPath websites are
separate browser activities governed by those services' policies:
[GitHub](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement)
and [SignPath](https://signpath.io/privacy-policy).

## Removing the application and data

Close Linthesia and delete its extracted portable folder to remove the Windows
application. Delete `%APPDATA%\Linthesia` if you also want to remove its saved
preferences and song statistics. If you configured a database elsewhere, remove
that separately if desired. Back up any personal MIDI files before removing
folders that contain them.

## Contact

Report application privacy concerns through the
[project issue tracker](https://github.com/DongwonSohn/linthesia-windows/issues).
Do not post private MIDI files, local paths, or other personal data in a public
issue.
