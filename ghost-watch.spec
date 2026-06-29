Name:           ghost-watch
Version:        1.0.0
Release:        1%{?dist}
Summary:        A simple and lightweight Niri screen time tracker for the terminal

License:        MIT
URL:            https://github.com/TheAK12/ghost-watch/
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  sqlite-devel
BuildRequires:  git
BuildRequires:  systemd-rpm-macros

Requires:       sqlite

%description
Ghost Watch is a screen time tracker for Niri
It features a native 60FPS rendering engine, SQLite persistence,
and an TUI design system.

%prep
%setup -c

%build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

%install
rm -rf %{buildroot}

install -Dm755 build/ghost-watch-tui %{buildroot}%{_bindir}/ghost-watch
install -Dm755 build/ghost-watch-daemon %{buildroot}%{_bindir}/ghost-watch-daemon

install -Dm644 ghost-watch.service %{buildroot}%{_userunitdir}/ghost-watch.service

%files
%{_bindir}/ghost-watch
%{_bindir}/ghost-watch-daemon
%{_userunitdir}/ghost-watch.service

%changelog
* Mon Jun 29 2026 Amritanshu Kumar <amritanshukumar13012008@gmaill.com> - 1.0.0-1
- Version 1.0 architecture freeze
- Integrated native mouse support, SQLite engine, and Apple UI
