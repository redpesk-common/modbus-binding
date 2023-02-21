Name: modbus-binding
Version: 1.1.0
Release: 1%{?dist}
Summary: Binding to serve an API connected to modbus hardware
Group:   Development/Libraries/C and C++
License:  Apache-2.0
URL: http://git.ovh.iot/redpesk/redpesk-common/modbus-binding
Source: %{name}-%{version}.tar.gz

BuildRequires:  afm-rpm-macros
BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  afb-cmake-modules
BuildRequires:  pkgconfig(json-c)
BuildRequires:  pkgconfig(lua) >= 5.3
BuildRequires:  pkgconfig(afb-binding)
BuildRequires:  pkgconfig(afb-libhelpers)
BuildRequires:  pkgconfig(afb-libcontroller)
BuildRequires:  pkgconfig(libsystemd) >= 222
BuildRequires:  pkgconfig(libmodbus) >= 3.1.6
Requires:       afb-binder

%description
%{name} Binding to serve an API connected to modbus hardware.

%package simulation
Summary:        Simulate a modbus tcp device

%description simulation
Simulate a modbus tcp device

%prep
%autosetup -p 1

%files
%afm_files

%files simulation
%{_afmdatadir}/%{name}/bin/*

%afm_package_test

%afm_package_devel

%build
%afm_configure_cmake
%afm_build_cmake

%install
%afm_makeinstall


%check

%clean

%%changelog

* Fri Jul 23 2021 IoT.bzh(iotpkg) <redpesk.list@iot.bzh> 1.1.0+20210711+6+g342f915
- Upgrade version from source commit sha: 342f915b5e6cdda9cbf17650cd70cdd04388bbc5
- Commit message:
- 	[Doc] Typo Redpesk/redpesk #2025
- 	
- 	Signed-off-by: Emilie Argouarch <emilie.argouarch@iot.bzh>
