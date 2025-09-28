# Notepad++ macOS Port

> Bringing the Notepad++ experience to macOS with a modern, native application.

This repository hosts the ongoing effort to deliver a native macOS edition of
Notepad++. The upstream project remains a Windows-focused text and source-code
editor; here we adapt its capabilities for macOS while maintaining the familiar
workflow and prioritizing contemporary security, accessibility, and UI/UX
expectations such as the Liquid Glass visual style.

## Project Overview

- **Modern platform layer** – replace Win32-specific filesystem, settings, and
  service integrations with portable abstractions ready for macOS.
- **Native user experience** – rebuild the interface with Qt 6, align with
  macOS design conventions, and provide Retina-quality rendering.
- **Robust validation** – expand automated and manual test coverage to guard
  against regressions as the codebase becomes cross-platform.
- **Documentation first** – evolve architecture, build, and rollout plans in
  lockstep with the implementation so contributors have a clear roadmap.

The detailed backlog lives in [`macOS_convert_todo.md`](macOS_convert_todo.md),
while completed activities are captured in
[`macOS_convert_tasks_done.md`](macOS_convert_tasks_done.md).

## Getting Started

Begin with the dedicated macOS documentation set:

- [Developer environment guide](docs/macos/developer_environment.md) – required
  tooling, repository layout, and validation gates.
- [Architecture assessment](docs/macos/architecture_assessment.md) – inventory
  of Windows-specific dependencies and proposed macOS counterparts.
- [UI strategy](docs/macos/ui_strategy.md) – design direction, toolkit
  selection, and Liquid Glass theming approach.
- [Build system plan](docs/macos/build_system_plan.md) – upcoming CMake targets,
  `.app` packaging flow, and CI integration strategy.
- [Port roadmap](docs/macos/port_roadmap.md) – phased milestones, readiness
  criteria, and success metrics.

## Building the Port

macOS build instructions are evolving alongside the platform work. Until the
new CMake targets land, reference the existing [build guide](BUILD.md) for the
Windows process and the [build system plan](docs/macos/build_system_plan.md) for
the macOS rollout strategy. Prototype builds currently rely on the dependencies
outlined in the developer environment guide.

## Quality & Testing

Robust validation is critical to reaching feature parity. The port introduces a
comprehensive unit and integration testing strategy, documented in the backlog
and associated design notes. Contributors are encouraged to add or extend tests
when touching platform abstractions, UI components, or editor features.

## Contributing

Contributions focused on the macOS port are welcome. Please review the
[Contribution Rules](CONTRIBUTING.md) and reference the planning documents above
when proposing changes so that new work aligns with the agreed roadmap.

## Upstream Project

The Windows release of Notepad++ remains available from the
[official site](https://notepad-plus-plus.org/) and is distributed under the
[GPL License](LICENSE). Supported Windows platforms are tracked in
[`SUPPORTED_SYSTEM.md`](SUPPORTED_SYSTEM.md). Details about the certificates used
to sign Windows releases are preserved below for reference.

### Notepad++ Root Certificate

_Since the release of version 8.8.3 Notepad++ is signed using code signing certificate issued by the following CA:_

- **Name:** Notepad++ Root Certificate
- **Serial Number:** 7A137FBEA48E8D469D2B43D49EBBCB21
- **Fingerprint:** C80539FF7076D22E73A01F164108DAFBF06E45E4
- **Created:** 2025-07-09
- **Expires:** 2055-07-09

Primary Location: https://github.com/notepad-plus-plus/notepad-plus-plus/blob/master/nppRoot.crt<br/>
Secondary Location: https://notepad-plus-plus.org/nppRoot.crt<br/>
Tertiary Location: https://npp-user-manual.org/docs/certs/nppRoot.crt

### Notepad++ Code Signing Certificate

_Since the release of version 8.8.3 Notepad++ is signed using code signing certificate:_

- **Name:** Notepad++
- **Emitted by:** Notepad++ Root Certificate
- **Serial Number:** 38D07732D5E4A2628A303D479035C1D1
- **Fingerprint:** 7F517E235584AFC146F6D3B44CD34C6CC36A3AB2
- **Created:** 2025-07-09
- **Expires:** 2028-07-09

### Notepad++ GPG Release Key

_Since the release of version 7.6.5 Notepad++ is signed using GPG with the following key:_

- **Signer:** Notepad++
- **E-mail:** don.h@free.fr
- **Key ID:** 0x8D84F46E
- **Key fingerprint:** 14BC E436 2749 B2B5 1F8C 7122 6C42 9F1D 8D84 F46E
- **Key type:** RSA 4096/4096
- **Created:** 2019-03-11
- **Expires:** 2027-03-13

https://github.com/notepad-plus-plus/notepad-plus-plus/blob/master/nppGpgPub.asc
