Name: modbus-config
Version: 1.0.0
Release: 0%{?dist}
Summary: modbus configuration for demonstration purpose

BuildArch: noarch

License: Apache-2.0
Source0: control-modbus-config.json

BuildRequires: afm-rpm-macros
Requires: modbus-binding

%description
%summary

%prep

%install
install -vd  %{buildroot}%{_afmdatadir}/modbus-binding/etc
install -m 0644 -vp %{SOURCE0} %{buildroot}%{_afmdatadir}/modbus-binding/etc/control-modbus-config.json

%files
%{_afmdatadir}/modbus-binding/etc/control-modbus-config.json

%define post_smack chsmack -a "App:modbus-binding:Conf" %{_afmdatadir}/modbus-binding/etc/*.json || :

%post
%post_smack

%changelog
