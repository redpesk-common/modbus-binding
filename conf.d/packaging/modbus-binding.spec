Name: modbus-binding
Version: 1.1.0
Release: 1%{?dist}
Summary: Binding to serve an API connected to modbus hardware
Group:   Development/Libraries/C and C++
License:  Apache-2.0
URL: https://github.com/redpesk-industrial/modbus-binding
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

%package seanatic-config
Summary:        config file for seanatic
Requires: %{name} = %{version}

%description seanatic-config
%summary

%package kingpigeon-config
Summary:        config file for kingpigeon
Requires: %{name} = %{version}

%description kingpigeon-config
%summary

%package raymarine-config
Summary:        config file for raymarine
Requires: %{name} = %{version}

%description raymarine-config
%summary

%prep
%autosetup -p 1

%afm_package_devel

%build
%afm_configure_cmake
%afm_build_cmake

%install
%afm_makeinstall

%files
%afm_files
%exclude %{_afmdatadir}/%{name}/etc/*

%files simulation
%{_afmdatadir}/bin/*

%files seanatic-config
%{_afmdatadir}/%{name}/etc/*seanatic*.json

%files kingpigeon-config
%{_afmdatadir}/%{name}/etc/*kingpigeon*.json

%files raymarine-config
%{_afmdatadir}/%{name}/etc/*raymarine*.json

%define post_smack chsmack -a "App:%{name}:Conf" %{_afmdatadir}/%{name}/etc/*.json || :

%post seanatic-config
%post_smack

%post kingpigeon-config
%post_smack

%post raymarine-config
%post_smack


%check

%clean

%changelog
