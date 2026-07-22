# Credits

PSPKVM REDUX builds directly on the work of several upstream projects.

## PSPKVM

The PSP port of Sun's phoneME Feature Java ME runtime.

- Developers: Sleepper, M@x, Anweifeng, AJ Milne
- Testing: Jurgen Konings
- Upstream mirror used here: https://github.com/vadosnaprimer/pspkvm
  (a fork of the original SourceForge project, http://sourceforge.net/projects/pspkvm)
- License: GPL-2.0

## phoneME Feature

The CLDC + MIDP Java ME implementation that PSPKVM ports.

- Sun Microsystems, Inc.
- License: GPL-2.0

## m3gcore

The Mobile 3D Graphics API (M3G, JSR-184) v1.1 core engine, in C. Originally
developed at Nokia Research Center Tampere (2003–2005) and released into open
source through the Symbian Foundation.

- Nokia Corporation
- Upstream: https://github.com/toaarnio/m3gcore
- License: Eclipse Public License v1.0

## Toolchain

- **pspdev / pspsdk** — the PSP homebrew cross-toolchain (https://github.com/pspdev)
- **Eclipse Temurin (Adoptium)** — JDK used for the phoneME host build tools
