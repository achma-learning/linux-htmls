# achma-tray.spec — built by `make rpm` from the same tree as the .deb.
# version_/release_ come from the Makefile; the defaults let a plain
# `rpmbuild -ba` from a tarball work too.
%{!?version_: %global version_ 1.0.0}
%{!?release_: %global release_ 1}
# Fedora defines this; plain rpm (e.g. building on Ubuntu) may not
%{!?_pkgdocdir: %global _pkgdocdir %{_docdir}/%{name}}

Name:           achma-tray
Version:        %{version_}
Release:        %{release_}%{?dist}
Summary:        Prayer day spine and AI chatbot launcher in the system tray
# the deck's own code is MIT; the prayer astronomy is a port of PrayTimes.org (LGPL)
License:        MIT AND LGPL-3.0-or-later
URL:            https://github.com/achma-learning/html-files
Source0:        %{name}-%{version}.tar.gz
BuildArch:      noarch
BuildRequires:  make

Requires:       python3 >= 3.8
Requires:       python3-gobject >= 3.30
Requires:       python3-cairo
Requires:       gtk3
Requires:       pango
# GNOME draws no tray without this extension; KDE and XFCE need nothing
Recommends:     (gnome-shell-extension-appindicator if gnome-shell)
Suggests:       gtk-layer-shell

%description
Two tray apps for Moroccan users, built from the pages in "html file/".

Prayer Spine (prayer-tray): city, weather, the day drawn to scale and the
countdown to the next prayer. Times are computed locally with the Moroccan
Ministry method, for 57 Moroccan cities or any custom location. Notifications
before and at each adhan.

AI Hub (ai-hub-tray): one click or Ctrl+Alt+A to open ChatGPT, Claude, Gemini
and a dozen more, each in its own window.

Uses the StatusNotifierItem tray protocol: works in KDE Plasma and XFCE as they
are, and in GNOME with the AppIndicator extension.

%prep
%autosetup

%build
# nothing to build: Python + GTK

%install
%make_install PREFIX=%{_prefix} DOCDIR=%{_pkgdocdir}

%files
%{_pkgdocdir}/README.md
%{_bindir}/prayer-tray
%{_bindir}/ai-hub-tray
%{_datadir}/%{name}/
%{_datadir}/applications/achma-prayer-tray.desktop
%{_datadir}/applications/achma-ai-hub-tray.desktop
%{_datadir}/icons/hicolor/scalable/apps/achma-prayer-tray.svg
%{_datadir}/icons/hicolor/scalable/apps/achma-ai-hub-tray.svg

%changelog
* Fri Oct 16 2026 achma-learning <achma-learning@users.noreply.github.com> - 1.0.0-1
- First Linux release: Prayer Spine and AI Hub tray apps
